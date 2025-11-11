#ifndef WCYDEVICESERVICE_H
#define WCYDEVICESERVICE_H

#include <QObject>
#include <types.h>

#include "sensorserviceclient.h"

class WCYDeviceService : public IDeviceService
{
    Q_OBJECT
public:
    WCYDeviceService();

    virtual ~WCYDeviceService() {}
    virtual void startScan();
    virtual void stopScan() {}
    virtual void deviceStartStream(const QString& deviceMac);
    virtual void deviceDisconnect(const QString& deviceMac) {}

    virtual void clearDevice() {}

    virtual const QList<DeviceInfo> getCurrentConnectDevice() { return QList<DeviceInfo>();}

private slots:
    void onConnected(bool connected);

private:
    QList<DeviceInfo> _devices;
    std::unique_ptr<WCYServiceWrapper> _wcyAPI;
};

#endif // WCYDEVICESERVICE_H
