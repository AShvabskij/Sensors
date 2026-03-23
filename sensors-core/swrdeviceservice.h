// swrdeviceservice.h
#ifndef SWRDEVICESERVICE_H
#define SWRDEVICESERVICE_H

#include "types.h"
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QSet>
#include <QtBluetooth>

#include "qbluetoothdevicemanagement.h"
#include "deviceapihandler.h"
#include "collector_OpenInterface_global.h"

class SWRDeviceService : public IDeviceService
{
    Q_OBJECT

public:
    explicit SWRDeviceService(QObject *parent = nullptr);
    virtual ~SWRDeviceService();

    // IDeviceService interface implementation
    virtual void init() override;
    virtual void startScan() override;
    virtual void stopScan() override;
    virtual void deviceConnect(const QString& deviceMac) override;
    virtual void deviceDisconnect(const QString& deviceMac) override;
    virtual void deviceStartStream(const QString& deviceMac) override;
    virtual void deviceStopStream(const QString& deviceMac) override;
    virtual void stopAll() override;
    virtual void exit() override;
    virtual const QList<DeviceInfo> getConnectedDeviceList() override;

private slots:
    void onDeviceStatusUpdated(swr::dInfo dInfo);
    void onDataReceived(swr::R_DATA_FLOW data);
    void onScanFinished();
    void onDeviceConnectionTimeout();

    // Слоты от DeviceAPIHandler
    void onCmdTransferRequired(QString deviceMac, QByteArray data);
    void onSensorInfoReceivedFromAPI(QString deviceMac, COLLECTOR_SensorInfo_C *info, uint64 count);
    void onDataReceivedFromAPI(QString deviceMac, COLLECTOR_DataLoggerDataInfo_C *data, uint64 count);

private:
    void processScannedDevice(const swr::dInfo &dInfo);
    DeviceInfo convertToDeviceInfo(const swr::dInfo &dInfo);
    swr::dInfo convertToSrcDeviceInfo(const DeviceInfo& deviceInfo);
    DeviceInfo* getDeviceByMac(const QString& deviceMac);
    void updateDeviceChannels(const QString& deviceMac, COLLECTOR_SensorInfo_C *sensorInfo, uint64 count);
    void processSensorData(const QString& deviceMac, COLLECTOR_DataLoggerDataInfo_C *dataInfo, uint64 count);
    void initializeBluetooth();
    void initializeCollectorAPI();
    void sendDataToDevice(const QString& deviceMac, const QByteArray& data);
    void updateDeviceInCollector(const DeviceInfo* device);
    DeviceAPIHandler* createAPIHandler(const QString& deviceMac);
    void deviceStopInternal(DeviceInfo *device);
    void deviceDisconnectInternal(DeviceInfo* device);
    void cleanup();
    void checkDeviceFreq(DeviceInfo *device);

private:
    QBluetoothDeviceManagement* m_bleManager;
    QMap<QString, DeviceInfo> m_devices;                    // deviceMac -> DeviceInfo
    QMap<QString, DeviceAPIHandler*> m_deviceAPIHandlers;   // deviceMac -> API Handler
    QMap<QString, QMap<int, COLLECTOR_SensorInfo_C>> m_deviceSensors; // Пока не нужен. deviceMac -> sensorInfo by HandleID.
    QMap<QString, QTimer*> m_connectionTimers;
    QRecursiveMutex m_devicesMutex;
    bool m_isInitialized;
    bool m_isScanning;
    QTimer* m_scanTimer;

    // Для отслеживания активных стримов
    QSet<QString> m_streamingDevices;

    // Для генерации timestamp
    QElapsedTimer m_timer;
    QMap<QString, QMap<int, qint64>> m_lastTimestamps; // deviceMac -> (channelId -> lastTimestamp)
    struct FrequencyStats {
        qint64 lastTimestamp = 0;
        int packetCount = 0;
        double currentFrequency = 0;
        QElapsedTimer measurementTimer;

        FrequencyStats() {
            measurementTimer.start();
        }
    };
    QMap<QString, FrequencyStats> m_frequencyStats; // deviceMac -> статистика частоты
    int m_frequencyUpdateInterval = 1000; // Интервал обновления частоты в мс (1 секунда)
};

#endif // SWRDEVICESERVICE_H
