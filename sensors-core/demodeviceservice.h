#ifndef DEMODEVICESERVICE_H
#define DEMODEVICESERVICE_H

#include <QFuture>
#include <QJsonArray>
#include <QObject>
#include <types.h>

class DemoDeviceService : public IDeviceService
{
    Q_OBJECT

public:
    DemoDeviceService();
    virtual ~DemoDeviceService(){};

    virtual void init() {};
    virtual void startScan();
    virtual void stopScan() {}
    virtual void deviceStartStream(const QString& deviceMac);
    virtual void deviceStopStream(const QString& deviceMac);

    virtual void deviceConnect(const QString& deviceMac) {};
    virtual void deviceDisconnect(const QString& deviceMac) {};

    virtual void stopAll();
    virtual void exit();

    virtual const QList<DeviceInfo> getConnectedDeviceList();

private:
    [[ noreturn ]] void thread_proc();
    DeviceInfo convertToDeviceInfo(const QJsonObject& devInfo);

    QJsonObject generateDevicesUpdate();
    QList<QJsonObject> generateData();

    QFuture<void> m_threadFuture;
    QJsonArray m_devices;

    bool m_dataEnabled = false;

};

#endif // DEMODEVICESERVICE_H
