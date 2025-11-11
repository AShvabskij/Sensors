#ifndef ICMDHANDLER_H
#define ICMDHANDLER_H

#include <QtWebSockets>

class IReqHandler : public QObject
{
    Q_OBJECT
public:
    virtual ~IReqHandler() {};

    virtual int handle(const QJsonObject& request) = 0;
    virtual bool canHandle(const QJsonObject &request) = 0;
    virtual void setNext(IReqHandler* next) = 0;
    virtual IReqHandler* next() = 0;

    virtual void handleClose() = 0;

    Q_SIGNAL void send(const QJsonObject& response);
    Q_SIGNAL void stream(const QList<QJsonObject>& valueList);
    Q_SIGNAL void stop_stream();
};

#endif // ICMDHANDLER_H
