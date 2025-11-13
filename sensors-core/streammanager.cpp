// streammanager.cpp
#include "streammanager.h"

#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>
#include <QDebug>
#include <QCborValue>

class StreamWorker : public QObject {
    Q_OBJECT
public:
    StreamWorker(QObject* parent = nullptr)
        : QObject(parent) {}

    void enqueue(const QList<QJsonObject>& list) {
        QMutexLocker locker(&m_mutex);
        for (const QJsonObject& obj : list) {
            m_queue.enqueue(obj);
        }
        m_waitCondition.wakeAll();
    }

    void clearQueue() {
        if (m_queue.empty()) return;

        QMutexLocker locker(&m_mutex);
        m_queue.clear();
    }

    void pause(int delay) {
        if (m_queue.empty()) return;

        QMutexLocker locker(&m_mutex);
        QThread::msleep(delay);
    }

public slots:
    void process() {
        while (true) {
            QMutexLocker locker(&m_mutex);
            if (m_queue.isEmpty()) {
                m_waitCondition.wait(&m_mutex);
                continue;
            }

            QJsonObject obj = m_queue.dequeue();
            locker.unlock();

            // QCborValue v = QCborValue::fromJsonValue(obj);
            // QByteArray dataToSend = v.toCbor(QCborValue::UseFloat);

            // emit sendMessage(dataToSend);

            emit sendData(obj);

//          QThread::msleep(10); // warn: not allowed here, because of data fragmentation
        }
    }

signals:
    void sendBinData(const QByteArray& data);
    void sendData(const QJsonObject& data);

private:
    QQueue<QJsonObject> m_queue;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
};

#include "streammanager.moc"

StreamManager::StreamManager()
    : m_totalBytes(0)
{
    m_worker = new StreamWorker();
    m_workerThread = new QThread();
    m_worker->moveToThread(m_workerThread);

    m_speedMeasurementTimer = new QTimer(this);
    connect(m_speedMeasurementTimer, &QTimer::timeout, this, &StreamManager::handlePing);
//  m_speedMeasurementTimer->start(10000);

    connect(m_workerThread, &QThread::started, m_worker, &StreamWorker::process);
    connect(m_worker, &StreamWorker::sendBinData, this, [this](const QByteArray& data){
        if (!m_isStarted) return;
        if (m_clients.isEmpty()) return;

        QWebSocket* client = m_clients.last();
        client->sendBinaryMessage(data);

        qint64 bytes = client->bytesToWrite();
        bool isSent = client->flush();
        // qDebug() << "Bytes sent:" << bytes;

        int NETWORK_COEF = (m_currSpeed == Fast) ? 100000 : (m_currSpeed == Medium) ? 1000 : 500; // the delay depends on network speed: 1000 - for local connection , 10000 - for remote vpn connection

        static int delay = bytes / NETWORK_COEF;
        if (isSent == true) {
            delay = delay - 0.1 * delay;
            delay = (delay <= 0) ? 0 : delay;
        } else {
            if (delay <= 0) {
                delay = bytes / NETWORK_COEF;
            }
            delay = delay + 0.1 * delay;
            delay = (delay <= 500) ? delay : 500; // delay not more than 500 ms
        }

        // need to delay after sending big chunks, to avoid traffic overflow
        if (delay < 10 || bytes < 1000 || isSent) {
            return;
        }

        qDebug() << "Streaming: " << "send bytes" << bytes << "with delay:" << delay;
        m_worker->pause(delay);
        QThread::msleep(delay);

        while (!isSent && bytes > 0) {
            bytes = client->bytesToWrite();
            isSent = client->flush();

            if (isSent == false) {
                delay = delay + 0.1 * delay;
            }

            m_worker->pause(delay);
            QThread::msleep(delay);
            qDebug() << "Streaming: " << "again send bytes" << bytes << "with delay:" << delay << "isSent" << isSent;

        }
    });

    connect(m_worker, &StreamWorker::sendData, this, [this](const QJsonObject& data){
        if (!m_isStarted) return;
        if (m_clients.isEmpty()) return;

        // QJsonDocument doc(data);
        // QTextStream(stdout) << "Sending data " << doc.toJson(QJsonDocument::Compact) << '\n';

        send(data);
    });

    m_workerThread->start();
}

StreamManager::~StreamManager() {
    m_workerThread->quit();
//  m_workerThread->wait(); // not allowed in destructor - app freezes when closing
    delete m_worker;
    QTextStream(stdout) << " Total bytes sended = " << m_totalBytes << "\n";
}

int StreamManager::stream(const QList<QJsonObject>& valueList) {
    if (valueList.isEmpty()) {

        m_currSpeed = checkCurrentSpeed();
    }

    m_isStarted = true;

    Q_ASSERT(m_worker);
    if (!m_worker) return -1;

    if (!m_clients.isEmpty()) {
        m_worker->enqueue(valueList);
    }

    return 0;
}

void StreamManager::stop_stream() {
    m_isStarted = false;
    m_currSpeed = Unknown;

    if (m_worker) {
        m_worker->clearQueue();
    }
    qDebug() << "Streaming: stopped";
}

void StreamManager::registerClient(QWebSocket* client) {
    if (!m_clients.contains(client)) {
        m_clients.enqueue(client);

        connect(client, &QWebSocket::pong, this, &StreamManager::handlePong);
    }
}

void StreamManager::unregisterClient(QWebSocket* client) {
    disconnect(client, &QWebSocket::pong, this, &StreamManager::handlePong);
    m_clients.removeAll(client);
}

int StreamManager::registerHandler(IReqHandler* handler) {
    QMetaObject::Connection con_stream = connect(handler, &IReqHandler::stream, this, &StreamManager::stream, Qt::QueuedConnection);
    QMetaObject::Connection con_stop_stream = connect(handler, &IReqHandler::stop_stream, this, &StreamManager::stop_stream, Qt::QueuedConnection);
    QMetaObject::Connection con_send = connect(handler, &IReqHandler::send, this, &StreamManager::send, Qt::QueuedConnection);

    if (!con_stream || !con_stop_stream || !con_send) {
        QTextStream(stdout) << "The stream connection is failed! " << '\n';
        return -1;
    }

    return 0;
}

void StreamManager::handlePing()
{
    if (m_clients.isEmpty()) return;

    QWebSocket *client = m_clients.last();
    if (client->state() != QAbstractSocket::ConnectedState) return;

    const int payloadSize = 120; // 120 B
    QByteArray payload;
    payload.reserve(payloadSize);

    // Fill the payload with some data (could be random or pattern-based)
    for (int i = 0; i < payloadSize; ++i) {
        payload.append(static_cast<char>(i % 256)); // Simple repeating pattern
    }

    client->ping(payload);
}

void StreamManager::handlePong(quint64 elapsedTime, const QByteArray& payload) {
    Q_UNUSED(payload);
    if (m_currentPingTime != elapsedTime) {
        m_currentPingTime = elapsedTime;
    }

    if (m_currentPingTime <= 100) m_currSpeed = Fast;
    if (m_currentPingTime > 100 && m_currentPingTime <=1000) m_currSpeed = Medium;
    if (m_currentPingTime > 1000) m_currSpeed = Slow;

    QString currSpeed = (m_currSpeed == Fast) ? "Fast" : (m_currSpeed == Medium) ? "Medium" : "Slow";
    qDebug() << "Streaming: " << "ping time" << elapsedTime << "ms" << "network speed = " << currSpeed;

}

StreamManager::NetworkSpeed StreamManager::checkCurrentSpeed() {
    handlePing();

    return m_currSpeed;
}

int StreamManager::send(QJsonObject data)
{
    if (data.empty()) return 0;

    QJsonDocument doc(data);
    QByteArray dataToSend = doc.toJson(QJsonDocument::Compact);
    bool needBinary = dataToSend.size() >= 10000;

    QString strDataToSend;
    if (needBinary) {
        QCborValue v = QCborValue::fromJsonValue(data);
        dataToSend = v.toCbor(QCborValue::UseFloat);
    } else {
        strDataToSend = dataToSend;
    }

    //    qDebug() << "Response:" << response;

    for (QWebSocket *client : m_clients) {
        if (needBinary) {
            client->sendBinaryMessage(dataToSend);
        } else {
            client->sendTextMessage(strDataToSend);
        }
        client->flush();
    }

    return 0;
}
