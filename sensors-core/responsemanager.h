#ifndef RESPONSEMANAGER_H
#define RESPONSEMANAGER_H

#include <QtWebSockets>
#include "ireqhandler.h"

class ResponseManager : public QObject
{
    Q_OBJECT

public:
    ResponseManager();
    ~ResponseManager()  override;

    enum MessageFormatEnum {
        JSON = 1, CBOR = 2
    };

    static ResponseManager* instance() {
        static ResponseManager i;
        return &i;
    }

    virtual void registerClient(QWebSocket* client);
    virtual void unregisterClient(QWebSocket* client);

    virtual int registerHandler(IReqHandler* handler);
    virtual int unregisterHandler(IReqHandler *handler);
    void terminate();
    virtual void clear();

public slots:
    int send(QJsonObject respObj);

private:

    QList<QWebSocket*> m_clients;
};

#endif // RESPONSEMANAGER_H
