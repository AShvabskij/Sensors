#include "responsemanager.h"

#include <cstdio>

ResponseManager::ResponseManager()
{

}

ResponseManager::~ResponseManager()
{
    qDebug() << "ResponseManager destroied";
}

int ResponseManager::send(QJsonObject respObj)
{
    if (respObj.empty()) return 0;

    QJsonDocument doc(respObj);
    QByteArray dataToSend = doc.toJson(QJsonDocument::Compact);
    bool needBinary = dataToSend.size() >= 10000;

    QString strDataToSend;
    if (needBinary) {
        QCborValue v = QCborValue::fromJsonValue(respObj);
        dataToSend = v.toCbor(QCborValue::UseFloat);
    } else {
        strDataToSend = dataToSend;
    }

//    qDebug() << "Response:" << response;

    for (QWebSocket *client : std::as_const(m_clients)) {
        if (needBinary) {
            client->sendBinaryMessage(dataToSend);
        } else {
            client->sendTextMessage(strDataToSend);
        }
        client->flush();
    }

    return 0;
}

void ResponseManager::registerClient(QWebSocket *client)
{
    if (m_clients.contains(client)) {
        return;
    }

    m_clients.append(client);
}

void ResponseManager::unregisterClient(QWebSocket *client)
{
    if (!m_clients.contains(client)) {
        return;
    }

    m_clients.removeOne(client);
}

int ResponseManager::registerHandler(IReqHandler *handler)
{
    connect(handler, &IReqHandler::send, this, &ResponseManager::send, Qt::QueuedConnection);
    return 0;
}

int ResponseManager::unregisterHandler(IReqHandler *handler)
{
    disconnect(handler, &IReqHandler::send, this, &ResponseManager::send);
    return 0;
}

void ResponseManager::terminate() {
    for (QWebSocket *client : std::as_const(m_clients)) {
        client->abort();
    }
}
void ResponseManager::clear() {
    terminate();
    for (QWebSocket *client : std::as_const(m_clients)) {
        client->deleteLater();
    }

    m_clients.clear();
    qDebug() << "response manager is cleared";
}
