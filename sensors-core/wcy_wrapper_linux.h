// wcy_wrapper_linux.h
#ifndef WCY_WRAPPER_LINUX_H
#define WCY_WRAPPER_LINUX_H

#include "iwcy_wrapper.h"
#include <QLibrary>
#include <QMutex>
#include <QElapsedTimer>
#include <dlfcn.h>

class WCY_Wrapper_Linux : public IWCY_Wrapper
{
    Q_OBJECT

public:
    explicit WCY_Wrapper_Linux(QObject *parent = nullptr);
    ~WCY_Wrapper_Linux();

    // IWCY_Wrapper interface
    virtual bool initialize() override;
    virtual void dispose() override;

    virtual void startBLEScan(int timeoutSeconds) override;
    virtual QVector<DeviceScan> startHIDScan() override;

    virtual bool connectDevice(const DeviceScan& device) override;
    virtual void disconnectDevice(const DeviceScan& device) override;

    virtual void startDataCollection(const DeviceScan& device) override;
    virtual void stopDataCollection(const DeviceScan& device) override;

    virtual QVector<ComboFetcher> getDataTypes(const DeviceScan& device) override;
    virtual bool setSensorLanguage(const DeviceScan& device, int langType) override;

    virtual QVector<SwitchRange> getSwitchRanges(const DeviceScan& device) override;
    virtual bool setSwitchRange(const DeviceScan& device, int rangeId) override;

    virtual int getCurrentFrequency(const DeviceScan& device) override;
    virtual QVector<Frequency> getSupportedFrequencies(const DeviceScan& device) override;
    virtual bool setFrequency(const DeviceScan& device, int frequency) override;

    virtual bool clearZero(const DeviceScan& device) override;
    virtual void shutdownDevice(const DeviceScan& device) override;

    virtual void setDeviceScanCallback(DeviceScanCallback callback) override;
    virtual void setDataReceivedCallback(DataReceivedCallback callback) override;
    virtual void setDeviceRemovedCallback(DeviceRemovedCallback callback) override;

private:
    // Статические callback-функции для C SDK
    static void deviceScanCallbackStatic(const DeviceScan* deviceScan);
    static void dataCallbackStatic(const DataCallback* dataCallback);
    static void deviceRemovedCallbackStatic(const DeviceScan& deviceScan);

    // Вспомогательные функции
    bool resolveFunctions();
    QString bytesToString(const char* bytes, int maxLength);
    void setupLibraryPath();

    // Типы указателей на функции SDK
    typedef bool (*InitializeFunc)();
    typedef void (*SetDeviceScanFunc)(DEVICE_SCAN_CALLBACK);
    typedef void (*BLEScanFunc)(int);
    typedef int (*HIDScanFunc)(DeviceScan**, int*);
    typedef void (*SetDataCallbackFunc)(DATA_CALLBACK);
    typedef void (*SetDeviceRemovedCallbackFunc)(DEVICE_REMOVED_CALLBACK);
    typedef bool (*ConnectFunc)(const DeviceScan*);
    typedef void (*DisConnectFunc)(const DeviceScan*);
    typedef void (*StartFunc)(const DeviceScan*);
    typedef void (*StopFunc)(const DeviceScan*);
    typedef int (*GetDataTypesFunc)(const DeviceScan*, ComboFetcher**, int*);
    typedef void (*FreeDataTypesFunc)(ComboFetcher*);
    typedef bool (*SetSensorLangFunc)(const DeviceScan*, int);
    typedef int (*GetSwitchRangesFunc)(const DeviceScan*, SwitchRange**, int*);
    typedef void (*FreeSwitchRangesFunc)(SwitchRange*);
    typedef bool (*SetSwitchRangeFunc)(const DeviceScan*, int);
    typedef int (*GetFrequencyFunc)(const DeviceScan*);
    typedef int (*GetSupportFrequencyFunc)(const DeviceScan*, Frequency**, int*);
    typedef void (*FreeSupportFrequencyFunc)(Frequency*);
    typedef bool (*SetFrequencyFunc)(const DeviceScan*, int);
    typedef bool (*ClearZeroFunc)(const DeviceScan*);
    typedef void (*ShutdownFunc)(const DeviceScan*);
    typedef void (*DisposeFunc)();

private:
    static WCY_Wrapper_Linux* m_instance;

    QLibrary* m_library;
    QElapsedTimer m_timer;
    QMutex m_callbackMutex;
    bool m_initialized;

    // Указатели на функции SDK
    InitializeFunc m_initialize;
    SetDeviceScanFunc m_setDeviceScan;
    BLEScanFunc m_bleScan;
    HIDScanFunc m_hidScan;
    SetDataCallbackFunc m_setDataCallback;
    SetDeviceRemovedCallbackFunc m_setDeviceRemovedCallback;
    ConnectFunc m_connect;
    DisConnectFunc m_disconnect;
    StartFunc m_start;
    StopFunc m_stop;
    GetDataTypesFunc m_getDataTypes;
    FreeDataTypesFunc m_freeDataTypes;
    SetSensorLangFunc m_setSensorLang;
    GetSwitchRangesFunc m_getSwitchRanges;
    FreeSwitchRangesFunc m_freeSwitchRanges;
    SetSwitchRangeFunc m_setSwitchRange;
    GetFrequencyFunc m_getFrequency;
    GetSupportFrequencyFunc m_getSupportFrequency;
    FreeSupportFrequencyFunc m_freeSupportFrequency;
    SetFrequencyFunc m_setFrequency;
    ClearZeroFunc m_clearZero;
    ShutdownFunc m_shutdown;
    DisposeFunc m_dispose;

    // Callbacks
    DeviceScanCallback m_deviceScanCallback;
    DataReceivedCallback m_dataReceivedCallback;
    DeviceRemovedCallback m_deviceRemovedCallback;
};

#endif // WCY_WRAPPER_LINUX_H
