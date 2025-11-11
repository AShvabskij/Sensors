#include "requestmanager.h"

#include <cstdio>

RequestManager::RequestManager()
{}

int RequestManager::processRequest(const QJsonObject& request)
{
//    if (!workerThread.isRunning()) {
//        Q_ASSERT(false);
//        return -1;
//    }

    QMetaObject::invokeMethod(this, "doProcessRequest",
                              Qt::QueuedConnection,
                              Q_ARG(QJsonObject, request));

    return 0;
}

int RequestManager::processClose()
{
    for (IReqHandler* handler : m_handlerList) {
            handler->handleClose();
    }

    return 0;
}

int RequestManager::remove(IReqHandler* handler)
{
    Q_ASSERT(handler);
    if (!handler) return -1;

    handler->handleClose();
    m_handlerList.removeAll(handler);

    for (IReqHandler* it : m_handlerList) {
        if (it->next() == handler) {
            it->setNext(nullptr);
        }
    }

    return 0;
}

int RequestManager::registerHandler(IReqHandler *handler)
{
    Q_ASSERT(handler);

//  handler->moveToThread(&workerThread);

    IReqHandler* lastHandler = !m_handlerList.isEmpty() ? m_handlerList.last() : nullptr;
    if (lastHandler) {
        lastHandler->setNext(handler);
    }

    m_handlerList.append(handler);

    return 0;
}

void RequestManager::start()
{
//    if (!workerThread.isRunning()) {
//        workerThread.start();
//    }

//    this->moveToThread(&workerThread);
}

void RequestManager::stop()
{
//    if (workerThread.isRunning()) {
//        workerThread.exit(0);
//    }
}

void RequestManager::doProcessRequest(const QJsonObject &request)
{
    IReqHandler* handler = !m_handlerList.isEmpty() ? m_handlerList.first() : nullptr;

    if (handler) {
        handler->handle(request);
    }
}

