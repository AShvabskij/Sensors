#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#include <QObject>
#include <QGrpcCallOptions>
#include <QGrpcChannelOptions>
#include <QtGrpc/QtGrpc>
#include <QtGrpc/QGrpcServerStream>
#include <QDebug>

#include "greet_client.grpc.qpb.h"

using namespace greet;

class GrpcClient : public QObject
{
    Q_OBJECT

public:
    explicit GrpcClient(QObject *parent = nullptr);
    ~GrpcClient();

public slots:
    void connectToServer(const QString &address = "http://localhost:5001");
    void sayHello(const QString &name);
    void startStream(const QString &name);
    void disconnectFromServer();

private:
    bool initializeClient(const QString &address);

signals:
    void messageReceived(const QString &message);
    void streamMessageReceived(const QString &message);
    void errorOccurred(const QString &error);
    void connectedChanged(bool connected);

// private slots:
//     void onSayHelloFinished();
//     void onStreamMessageReceived();
//     void onStreamError(const QGrpcStatus &status);

private:
    QUrl m_hostUri;
    QGrpcChannelOptions m_channelOptions;

    // Используем unique_ptr на Client (как в примере Qt)
    std::unique_ptr<greet::Greeter::Client> m_client;

    // Для стрима — держим shared_ptr на QGrpcStream
    std::unique_ptr<QGrpcServerStream> m_stream;

    bool m_connected;
};

#endif // GRPC_CLIENT_H
