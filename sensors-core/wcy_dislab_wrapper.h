#ifndef WCY_DISLAB_WRAPPER_H
#define WCY_DISLAB_WRAPPER_H

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include "WCY_DisLab_SDK.h"
#include "qelapsedtimer.h"

// Структуры данных (повторяем структуры из SDK)
class WCY_DisLab_Wrapper : public QObject
{
    Q_OBJECT

public:
    explicit WCY_DisLab_Wrapper(QObject *parent = nullptr);
    ~WCY_DisLab_Wrapper();

    struct RData
    {
        long long timestamp = 0;
        int rangeId = 0;
        double value = 0;
    };

    // Основные методы
    bool initialize();
    void dispose();

    void startBLEScan(int timeoutSeconds);
    QVector<DeviceScan> startHIDScan();

    bool connectDevice(const DeviceScan& device);
    void disconnectDevice(const DeviceScan& device);

    void startDataCollection(const DeviceScan& device);
    void stopDataCollection(const DeviceScan& device);

    QVector<ComboFetcher> getDataTypes(const DeviceScan& device);
    bool setSensorLanguage(const DeviceScan& device, int langType);

    QVector<SwitchRange> getSwitchRanges(const DeviceScan& device);
    bool setSwitchRange(const DeviceScan& device, int rangeId);

    int getCurrentFrequency(const DeviceScan& device);
    QVector<Frequency> getSupportedFrequencies(const DeviceScan& device);
    bool setFrequency(const DeviceScan& device, int frequency);

    bool clearZero(const DeviceScan& device);
    void shutdownDevice(const DeviceScan& device);

    // Callbacks
    using DeviceScanCallback = std::function<void(const DeviceScan&)>;
    using DataReceivedCallback = std::function<void(const DataCallback&)>;
    using DeviceRemovedCallback = std::function<void(const DeviceScan&)>;

    void setDeviceScanCallback(DeviceScanCallback callback);
    void setDataReceivedCallback(DataReceivedCallback callback);
    void setDeviceRemovedCallback(DeviceRemovedCallback callback);

public slots:
    void handleDataReceived(const DataCallback& data);

signals:
    void deviceScanned(const DeviceScan& device);
    void dataReceived(const WCY_DisLab_Wrapper::RData& data);
    void deviceRemoved(const DeviceScan& device);
    void errorOccurred(const QString& errorMessage);

private:
    // Статические callback-функции для C SDK
    static void deviceScanCallbackStatic(const DeviceScan* deviceScan);
    static void dataCallbackStatic(const DataCallback* dataCallback);
    static void deviceRemovedCallbackStatic(const DeviceScan &deviceScan);

    // Указатели на функции обратного вызова
    static WCY_DisLab_Wrapper* instance;
    QElapsedTimer m_timer;
    DeviceScanCallback deviceScanCallback;
    DataReceivedCallback dataReceivedCallback;
    DeviceRemovedCallback deviceRemovedCallback;

private:
    QMutex m_callbackMutex;
};

#endif // WCY_DISLAB_WRAPPER_H
