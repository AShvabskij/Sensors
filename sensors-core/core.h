#ifndef CORE_H
#define CORE_H

#include "types.h"

#include "socketserver.h"

#include "basereqhandler.h"

#include "sensorserviceclient.h"

class IDDE;
class IDDE_Dispatcher;
class DataUsbCopier;

class Core: public BaseReqHandler
{
    Q_OBJECT
public:
    Core();
    ~Core();

    virtual int handle(const QJsonObject &request);

    void init();
    void start();

private slots:
    void onSystemInit();

private:
    [[ noreturn ]] void thread_proc();

    void handleSystemInit(const QJsonObject& request);
    _return_t executeScript(const QString& script);

    QJsonObject generateDevicesUpdate();
    QList<QJsonObject> generateData();
    QJsonObject convertInfoToDTO(DeviceInfo dev);
    QJsonObject convertFlowToDTO(DataFlow data);

    void TestSensorRPC();

    SocketServer* m_cmdServer = nullptr;
    SocketServer* m_streamServer = nullptr;

    QFuture<void> m_threadFuture;
    QMutex m_mutex;
    QTimer* m_startTimer;
    bool m_isActivated = false;

    QJsonArray _devices;

    DeviceModel _connectedDevice;
    std::unique_ptr<WCYServiceWrapper> _wcyAPI;

    IDeviceService* _devService;
};

#endif // CORE_H
