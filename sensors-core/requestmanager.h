#ifndef REQUESTMANAGER_H
#define REQUESTMANAGER_H

#include <QtWebSockets>
#include <QQueue>

#include "ireqhandler.h"

class RequestManager : public QObject
{
    Q_OBJECT
public:
    RequestManager();

    static RequestManager* instance() {
        static RequestManager i;
        return &i;
    }

    int processRequest(const QJsonObject& request);
    int processClose();
    int registerHandler(IReqHandler* handler);
    void start();
    void stop();
    int remove(IReqHandler* handler);

private slots:
    void doProcessRequest(const QJsonObject &request);

private:

//  QThread workerThread;
    QQueue<QJsonObject> m_requests;
    QList<IReqHandler *> m_handlerList;
};

#endif // REQUESTMANAGER_H
