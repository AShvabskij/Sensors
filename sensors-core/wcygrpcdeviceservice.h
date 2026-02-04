#ifndef WCYGRPCDEVICESERVICE_H
#define WCYGRPCDEVICESERVICE_H

#include <QObject>
#include <types.h>

#include "qjsonarray.h"
#include "qprocess.h"
#include "sensor_service_client.grpc.qpb.h"

class WCYServiceWrapper;

class WCYGRPCDeviceService : public IDeviceService
{
    Q_OBJECT
public:
    WCYGRPCDeviceService();

    virtual ~WCYGRPCDeviceService();

    virtual void init();
    virtual void startScan();
    virtual void stopScan() {}
    virtual void deviceStartStream(const QString& deviceMac);
    virtual void deviceStopStream(const QString& deviceMac);

    virtual void deviceConnect(const QString& deviceMac);
    virtual void deviceDisconnect(const QString& deviceMac);

    virtual void stopAll();
    virtual void exit();

    virtual const QList<DeviceInfo> getConnectedDeviceList();

signals:
    void prescanCompleted();
    void bleScanUncompleted();
    void stopCompleted();

private:
    void startDeviceScan(CONNECT_TYPE connectType);
    void connectNextDevice();
    void connectToDevice(const QString& deviceMac);
    void handleConnectionTimeout(const QString &deviceMac);
    DeviceInfo* getDeviceByMac(const QString &deviceMac);
    bool loadUnitsFromJson(QJsonArray& _units);
    long startSensorServer();
    long stopSensorServer();
    bool stopProcessByNameDirect(const QString& processName);

private slots:
    void onDeviceConnected(const QString &deviceMac, GrpcSensor::ConnectEnumGadget::ConnectEnum result);
    void onSensorRangeReceived(const QString &mac, QList<GrpcSensor::DataTypeModel> ranges);

private:
    QList<DeviceInfo> _devices;
    std::unique_ptr<WCYServiceWrapper> _wcyAPI;
    CONNECT_TYPE _currConnectType = UNKNOWN;
    bool _isServerConnected = false;
    QJsonArray _units;
    QProcess m_process;

    struct ConnectionHolder {
        QMetaObject::Connection _serverConnect;
        QMetaObject::Connection _scanCompleted;
        QMetaObject::Connection _deviceConnected;
        QMetaObject::Connection _sensorRangeReceived;
        QMetaObject::Connection _sensorDataReceived;
        QMetaObject::Connection _sensorEventReceived;

        void disconnectAll() {
            QObject::disconnect(_serverConnect);
            QObject::disconnect(_deviceConnected);
            QObject::disconnect(_sensorRangeReceived);
            QObject::disconnect(_sensorDataReceived);
            QObject::disconnect(_sensorEventReceived);
        }
    };

    ConnectionHolder _connections;
};

#endif // WCYGRPCDEVICESERVICE_H
