#ifndef WCYDEVICESERVICE_H
#define WCYDEVICESERVICE_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "wcy_dislab_wrapper.h"
#include "types.h"

class WCYDeviceService : public IDeviceService
{
    Q_OBJECT

public:
    explicit WCYDeviceService();
    virtual ~WCYDeviceService();

    // IDeviceService interface implementation
    virtual void init() override;
    virtual void startScan() override;
    virtual void stopScan() override;
    virtual void deviceStartStream(const QString& deviceMac) override;
    virtual void deviceStopStream(const QString& deviceMac) override;
    virtual void deviceConnect(const QString& deviceMac) override;
    virtual void deviceDisconnect(const QString& deviceMac) override;
    virtual void stopAll() override;
    virtual void exit() override;
    virtual const QList<DeviceInfo> getConnectedDeviceList() override;

private:
    // Internal methods
    void startBLEScan();
    void startHIDScan();
    void processScannedDevice(const DeviceScan& deviceScan, CONNECT_TYPE connectType);
    void connectNextDevice();
    _return_t tryDeviceConnect(DeviceInfo *device);
    _return_t setPreferedFrequency(const DeviceScan &device, int frequency);
    void handleConnectionFailed(const QString& deviceMac);
    void handleConnectionTimeOut(const QString& deviceMac);
    DeviceInfo* getDeviceByMac(const QString& deviceMac);
    DeviceInfo* getDeviceByName(const QString& deviceName);
    _return_t updateDeviceChannels(DeviceInfo* device);
    bool loadUnitsFromJson();
    void stopStreamingAll();

    // Conversion helpers
    DeviceInfo convertToDeviceInfo(const DeviceScan& deviceScan, CONNECT_TYPE connectType);
    DeviceScan convertToDeviceScan(const DeviceInfo* device) const;

    // Callback handlers
    void onDeviceScanned(const DeviceScan& deviceScan);
    void onDataReceived(const WCY_DisLab_Wrapper::RData &data);
    void onDeviceRemoved(const DeviceScan& deviceScan);
    void onErrorOccurred(const QString& errorMessage);

private:
    WCY_DisLab_Wrapper* m_wrapper;
    QList<DeviceInfo> m_devices;
    QSet<QString> m_scannedDevices;

//  QMap<QString, QTimer*> m_connectionTimers;
    QJsonArray m_channelsDescr;
    QRecursiveMutex m_devicesMutex;
    bool m_isInitialized;
    bool m_isScanning;
    CONNECT_TYPE m_currentScanType;
    QMap<int/*rangeId*/, DeviceInfo*> m_ranges;
    QMap<int/*rangeId*/,  int64> m_lastTimestamp;

    QElapsedTimer m_timer;
    struct {
        bool bleScanCompleted;
        bool hidScanCompleted;
    } m_scanStatus;
};

#endif // WCYDEVICESERVICE_H
