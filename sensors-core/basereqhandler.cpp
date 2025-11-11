#include "basereqhandler.h"


int BaseReqHandler::handle(const QJsonObject &request)
{
    if (m_next != nullptr) {
        m_next->handle(request);
    }

    return 1;
}

bool BaseReqHandler::canHandle(const QJsonObject &request)
{
    int requestId = request.value("request_id").toInt();

    if (requestId <= 0) {
        return false;
    }

    return true;
}

void BaseReqHandler::setNext(IReqHandler *next)
{
    m_next = next;
}

IReqHandler *BaseReqHandler::next()
{
    return m_next;
}

void BaseReqHandler::setResponseManager(ResponseManager* response)
{
    m_response = response;
}


QJsonObject BaseReqHandler::createEmptyResponse(int requestId)
{
    QJsonObject res;
    res["request_id"] = requestId;
    res["body"] = QJsonArray();

    return res;
}
