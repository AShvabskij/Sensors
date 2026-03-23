#ifndef CORE_H
#define CORE_H

#include "types.h"

#include "socketserver.h"

#include "basereqhandler.h"

class IDDE;
class IDDE_Dispatcher;
class DataUsbCopier;

class Core: public BaseReqHandler // todo: it's better to create a separate handler
{
    Q_OBJECT
public:
    Core();
    ~Core();

    virtual int handle(const QJsonObject &request);

    void init();
    void start(SysType sysType);
    void stop();

signals:
    void stopCompleted();

private:
//    [[ noreturn ]] void thread_proc();

    void handleSystemInit(const QJsonObject& request);
    _return_t executeScript(const QString& script);

    void registerSensorService(SysType sysType, IDeviceService* service);

    QJsonObject convertInfoToDTO(QList<DeviceInfo> dev_list);
    QJsonObject convertDataFlowToDTO(DataFlow data);

    QList<SysType> m_supportedSysTypes;
    QMap<SysType, IDeviceService*> m_services;

    SocketServer* m_cmdServer = nullptr;
    SocketServer* m_streamServer = nullptr;

    QFuture<void> m_threadFuture;
    QMutex m_mutex;
    QTimer* m_scanTimer = nullptr;
    bool m_isActivated = false;

    QJsonArray m_devices;

    QProcess m_process;

    IDeviceService* _devService = nullptr;

};

#endif // CORE_H
