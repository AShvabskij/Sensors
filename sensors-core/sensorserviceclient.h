#ifndef SENSOR_SERVICE_CLIENT_H
#define SENSOR_SERVICE_CLIENT_H

#include <QObject>
#include <QGrpcCallOptions>
#include <QGrpcChannelOptions>
#include <QtGrpc/QtGrpc>
#include <QtGrpc/QGrpcServerStream>
#include <QDebug>

#include "sensor_service_client.grpc.qpb.h"

using namespace GrpcSensor;

class WCYServiceWrapper : public QObject
{
    Q_OBJECT

public:
    explicit WCYServiceWrapper(QObject *parent = nullptr);
    ~WCYServiceWrapper();

public slots:
    void connectToServer(const QString &address = "http://localhost:50051");
    void disconnectFromServer();

    // Scanning methods
    void scanBLE(int32_t timeout);
    void scanUSB();

    // Connection methods
    void connectDevice(const QString &mac);
    void disconnectDevice(const QString &deviceMac);
    void deviceExit(const QString &deviceMac);

    // Data operations
    void getSensorRange(const QString &deviceMac);
    void startSendData(const QString &deviceMac, const QList<int32_t> &rangeIdList);
    void stopSendData(const QString &deviceMac);
    void clearZero(const QString &deviceMac, const QList<int32_t> &rangeIdList);

    // Configuration methods
    void setSensorLang(const QString &deviceMac, int32_t langType);
    void getSwitchRange(const QString &deviceMac);
    void setRange(const QString &deviceMac, int32_t rangeId);
    void getCurrentFrequency(const QString &deviceMac);
    void getSupportFrequency(const QString &deviceMac);
    void setFrequency(const QString &deviceMac, int32_t frequency);

    // Streaming methods
    void subscribeToSensorData(const QString &deviceMac);
    void subscribeToSensorEvents(const QString &deviceMac);

signals:
    void connectedChanged(bool connected);
    void errorOccurred(const QString &error);
    void messageReceived(const QString &message);

    // Scanning signals
    void devScanFinished(const QList<DeviceModel> &devices, const QString &error);

    // Connection signals
    void deviceConnected(ConnectEnumGadget::ConnectEnum result, const QString &error);
    void deviceDisconnected(bool success, const QString &error);
    void deviceExited(bool success, const QString &error);

    // Data operations signals
    void sensorRangeReceived(const QList<DataTypeModel> &ranges, const QString &error);
    void startSendDataFinished(bool success, const QString &error);
    void stopSendDataFinished(bool success, const QString &error);
    void clearZeroFinished(bool success, const QString &error);

    // Configuration signals
    void sensorLangSet(bool success, const QString &error);
    void switchRangeReceived(const QList<SwitchRangeModel> &ranges, const QString &error);
    void rangeSet(bool success, const QString &error);
    void currentFrequencyReceived(int32_t frequency, const QString &error);
    void supportFrequencyReceived(const QList<FrequencyModel> &frequencies, const QString &error);
    void frequencySet(bool success, const QString &error);

    // Streaming signals
    void sensorDataReceived(const QList<SensorDataModel> &sensorData);
    void sensorEventReceived(const QString &deviceMac, const QString &eventType, const QString &message);

private:
    bool initializeClient(const QString &address);

private:
    QUrl m_hostUri;
    QGrpcChannelOptions m_channelOptions;

    std::unique_ptr<SensorService::Client> m_client;
    std::unique_ptr<QGrpcServerStream> m_sensorDataStream;
    std::unique_ptr<QGrpcServerStream> m_sensorEventsStream;

    bool m_connected;
};

#endif // SENSOR_SERVICE_CLIENT_H
