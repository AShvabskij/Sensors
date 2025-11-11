#include "grpc_client.h"

#include <QtCore/QUrl>
#include <QtGrpc/QGrpcChannelOptions>
#include <QtGrpc/QGrpcHttp2Channel>
#include <QtCore/QDebug>
#include <QtCore/QUrl>
#include <QtGrpc/QGrpcServerStream>
#include <QtGrpc/QGrpcCallReply>
#include <QtGrpc/QGrpcStatus>
#include <QtGrpc/QGrpcHttp2Channel>
#include <QtGrpc/QGrpcChannelOptions>
#include <QtGrpc/QGrpcCallOptions>

using namespace std::chrono_literals;

GrpcClient::GrpcClient(QObject *parent)
    : QObject(parent)
    , m_connected(false)
{
    // Настройка канала
    m_channelOptions = QGrpcChannelOptions();
}

GrpcClient::~GrpcClient()
{
    disconnectFromServer();
}

bool GrpcClient::initializeClient(const QString &address)
{
    QUrl m_hostUri(address);
    if (!m_hostUri.isValid()) {
        emit errorOccurred(QString("Неверный адрес: %1").arg(address));
        return false;
    }

    QGrpcChannelOptions opts = m_channelOptions;
    // при необходимости: настройка SSL/ALPN и т.д. (как в примере QtGrpcChat)

    m_client = std::make_unique<greet::Greeter::Client>();
    // attachChannel возвращает bool — проверяем успех
    if (!m_client->attachChannel(std::make_shared<QGrpcHttp2Channel>(m_hostUri, opts))) {
        qWarning() << "Failed to attach channel to gRPC client";
        m_client.reset();
        return false;
    }

    return true;
}

void GrpcClient::connectToServer(const QString &address)
{
    try {
        if (!initializeClient(address)) {
            emit errorOccurred(QString("Ошибка инициализации gRPC-клиента для %1").arg(address));
            return;
        }

        m_connected = true;
        emit connectedChanged(true);
        emit messageReceived("Успешно подключено к серверу: " + address);

        qDebug() << "Connected to gRPC server at:" << address;

    } catch (const std::exception &e) {
        m_connected = false;
        emit connectedChanged(false);
        emit errorOccurred(QString("Ошибка подключения: %1").arg(e.what()));
        qCritical() << "Connection error:" << e.what();
    }
}

void GrpcClient::sayHello(const QString &name)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Клиент не подключен к серверу");
        return;
    }

    try {
        greet::HelloRequest request;
        request.setName(name);

        qDebug() << "Sending SayHello request for:" << name;

        // Асинхронно получаем QGrpcCallReply (сигнал finished будет вызван при завершении)
        // Можно задать таймаут через QGrpcCallOptions если нужно: .setDeadlineTimeout(5s)
        auto reply = m_client->SayHello(request, QGrpcCallOptions{}.setDeadlineTimeout(5s));
        const auto *replyPtr = reply.get();

        connect(replyPtr, &QGrpcCallReply::finished, this,
            [this, reply = std::move(reply)](const QGrpcStatus &status) mutable {
                if (status.isOk()) {
                    greet::HelloReply response;
                    if (reply->read(&response)) {
                        QString resp = response.message();
                        emit messageReceived("Ответ от сервера: " + resp);
                        qDebug() << "SayHello response:" << resp;
                    } else {
                        emit errorOccurred("Не удалось десериализовать ответ SayHello");
                        qWarning() << "Failed to read SayHello response";
                    }
                } else {
                    emit errorOccurred(QString("Ошибка RPC: %1").arg(status.message()));
                    qWarning() << "RPC error:" << status.message();
                }
            },
            Qt::SingleShotConnection);

    } catch (const std::exception &e) {
        emit errorOccurred(QString("Исключение при вызове SayHello: %1").arg(e.what()));
        qCritical() << "Exception in sayHello:" << e.what();
    }
}

void GrpcClient::startStream(const QString &name)
{
    if (!m_connected || !m_client) {
        emit errorOccurred("Клиент не подключен к серверу");
        return;
    }

    greet::HelloRequest request;
    request.setName(name);

    qDebug() << "Starting server stream for:" << name;

    // Новый API в Qt 6.10: возвращается QGrpcServerStream (вместо QGrpcStream)
    m_stream = m_client->SayHelloStream(request, QGrpcCallOptions{});

    if (!m_stream) {
        emit errorOccurred("Не удалось запустить стрим (пустой поток)");
        return;
    }

    // Обрабатываем входящие сообщения
    connect(m_stream.get(), &QGrpcServerStream::messageReceived, this,
            [this]() mutable {
                greet::HelloReply reply;
                if (!m_stream->read(&reply))
                    return;
                QString text = reply.message();
                emit streamMessageReceived("Стрим: " + text);
                qDebug() << "Stream message:" << text;
            });

    // Обработка завершения стрима
    connect(m_stream.get(), &QGrpcServerStream::finished, this,
            [this](const QGrpcStatus &status) {
                if (status.isOk()) {
                    emit messageReceived("Стрим завершён успешно");
                } else {
                    emit errorOccurred(QString("Ошибка стрима: %1").arg(status.message()));
                }
            });


    emit messageReceived("Стрим запущен для: " + name);
}

void GrpcClient::disconnectFromServer()
{
    // Заканчиваем стрим если есть
    if (m_stream) {
        // если stream поддерживает writesDone() — можно вызвать, если это bidi
        // m_stream->writesDone(); // только при двунаправленном стриме
        m_stream.reset();
    }

    if (m_client) {
        m_client.reset();
    }

    m_connected = false;
    emit connectedChanged(false);
    emit messageReceived("Отключено от сервера");
    qDebug() << "Disconnected from server";
}
