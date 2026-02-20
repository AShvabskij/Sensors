// iwcy_wrapper.h
#ifndef IWCY_WRAPPER_H
#define IWCY_WRAPPER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include "WCY_DisLab_SDK.h"

class IWCY_Wrapper : public QObject
{
    Q_OBJECT

public:
    struct RData
    {
        long long timestamp = 0;
        int rangeId = 0;
        double value = 0;
    };

    // Явно объявляем конструктор с параметром parent
    explicit IWCY_Wrapper(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IWCY_Wrapper() {}

    // Основные методы
    virtual bool initialize() = 0;
    virtual void dispose() = 0;

    virtual void startBLEScan(int timeoutSeconds) = 0;
    virtual QVector<DeviceScan> startHIDScan() = 0;

    virtual bool connectDevice(const DeviceScan& device) = 0;
    virtual void disconnectDevice(const DeviceScan& device) = 0;

    virtual void startDataCollection(const DeviceScan& device) = 0;
    virtual void stopDataCollection(const DeviceScan& device) = 0;

    virtual QVector<ComboFetcher> getDataTypes(const DeviceScan& device) = 0;
    virtual bool setSensorLanguage(const DeviceScan& device, int langType) = 0;

    virtual QVector<SwitchRange> getSwitchRanges(const DeviceScan& device) = 0;
    virtual bool setSwitchRange(const DeviceScan& device, int rangeId) = 0;

    virtual int getCurrentFrequency(const DeviceScan& device) = 0;
    virtual QVector<Frequency> getSupportedFrequencies(const DeviceScan& device) = 0;
    virtual bool setFrequency(const DeviceScan& device, int frequency) = 0;

    virtual bool clearZero(const DeviceScan& device) = 0;
    virtual void shutdownDevice(const DeviceScan& device) = 0;

    // Callbacks
    using DeviceScanCallback = std::function<void(const DeviceScan&)>;
    using DataReceivedCallback = std::function<void(const DataCallback&)>;
    using DeviceRemovedCallback = std::function<void(const DeviceScan&)>;

    virtual void setDeviceScanCallback(DeviceScanCallback callback) = 0;
    virtual void setDataReceivedCallback(DataReceivedCallback callback) = 0;
    virtual void setDeviceRemovedCallback(DeviceRemovedCallback callback) = 0;

signals:
    void deviceScanned(const DeviceScan& device);
    void dataReceived(const IWCY_Wrapper::RData& data);
    void deviceRemoved(const DeviceScan& device);
    void errorOccurred(const QString& errorMessage);
};

#endif // IWCY_WRAPPER_H
