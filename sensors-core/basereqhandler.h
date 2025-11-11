#ifndef BASEREQHANDLER_H
#define BASEREQHANDLER_H

#include "ireqhandler.h"
#include "responsemanager.h"

class BaseReqHandler : public IReqHandler
{
    Q_OBJECT
public:
    BaseReqHandler() {};

    ~BaseReqHandler() {};

    virtual int handle(const QJsonObject& request);
    virtual bool canHandle(const QJsonObject &request);
    virtual void setNext(IReqHandler* next);
    virtual IReqHandler* next();
    void setResponseManager(ResponseManager* response);
    virtual void handleClose() {};

protected:
    QJsonObject createEmptyResponse(int requestId);

    IReqHandler* m_next = nullptr;
    ResponseManager* m_response = nullptr;

};

#endif // BASEREQHANDLER_H
