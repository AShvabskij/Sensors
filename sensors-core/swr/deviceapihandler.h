// deviceapihandler.h (исправленная версия)
#ifndef DEVICEAPIHANDLER_H
#define DEVICEAPIHANDLER_H

#include <QObject>
#include <QLibrary>
#include <QMutex>
#include <QMap>
#include <QThread>
#include "collector_OpenInterface_global.h"

class DeviceAPIHandler : public QObject
{
    Q_OBJECT

public:
    explicit DeviceAPIHandler(const QString& deviceMac, QObject *parent = nullptr);
    ~DeviceAPIHandler();

    bool initialize();
    void shutdown();

    QString deviceMac() const { return m_deviceMac; }

    // API функции
    void updateDevice(const COLLECTOR_DataLoggerInfo_C& info);
    void startContinuouslyCollect();
    void stopCollect();
    void startSingleCollect();
    void setCollectInterval(COLLECTOR_INTERVAL_C interval);
    void getSensorInfo();
    void demarcateSensor(const COLLECTOR_DemarcateInfo_C& info);
    void calibrationSensor(const COLLECTOR_CalibrationInfo_C& info);
    void appendDeviceData(const COLLECTOR_R_DATA_FLOW_C& data);

    // Функции глубокого копирования структур
    static COLLECTOR_SensorInfo_C deepCopySensorInfo(const COLLECTOR_SensorInfo_C& source)
    {
        COLLECTOR_SensorInfo_C dest;

        // Копируем m_DeviceMac (20 байт)
        memcpy(dest.m_DeviceMac, source.m_DeviceMac, 20);

        // Копируем простые типы
        dest.m_Channel = source.m_Channel;
        dest.m_HandleID = source.m_HandleID;
        dest.m_Decimal = source.m_Decimal;
        dest.m_MinValue = source.m_MinValue;
        dest.m_MaxValue = source.m_MaxValue;

        // Копируем строки m_Name и m_Unit (100 байт каждая)
        memcpy(dest.m_Name, source.m_Name, 100);
        memcpy(dest.m_Unit, source.m_Unit, 100);

        return dest;
    }

    static QVector<COLLECTOR_SensorInfo_C> deepCopySensorInfoArray(const COLLECTOR_SensorInfo_C* source, uint64 count)
    {
        QVector<COLLECTOR_SensorInfo_C> dest;
        dest.reserve(static_cast<int>(count));

        for (uint64 i = 0; i < count; ++i) {
            dest.append(deepCopySensorInfo(source[i]));
        }

        return dest;
    }

    static COLLECTOR_DataLoggerDataInfo_C deepCopyDataInfo(const COLLECTOR_DataLoggerDataInfo_C& source)
    {
        COLLECTOR_DataLoggerDataInfo_C dest;

        // Копируем простые типы
        dest.m_IsSingleCollect = source.m_IsSingleCollect;
        dest.m_HandleID = source.m_HandleID;
        dest.m_Value = source.m_Value;
        dest.m_Upside = source.m_Upside;
        dest.m_Index = source.m_Index;
        dest.m_GDMTime = source.m_GDMTime;

        // Копируем массив m_ADValueArray (5 элементов)
        for (int j = 0; j < 5; ++j) {
            dest.m_ADValueArray[j] = source.m_ADValueArray[j];
        }

        return dest;
    }

    static QVector<COLLECTOR_DataLoggerDataInfo_C> deepCopyDataInfoArray(const COLLECTOR_DataLoggerDataInfo_C* source, uint64 count)
    {
        QVector<COLLECTOR_DataLoggerDataInfo_C> dest;
        dest.reserve(static_cast<int>(count));

        for (uint64 i = 0; i < count; ++i) {
            dest.append(deepCopyDataInfo(source[i]));
        }

        return dest;
    }

signals:
    void cmdTransferRequired(QString deviceMac, QByteArray data);
    void sensorInfoReceived(QString deviceMac, COLLECTOR_SensorInfo_C *info, uint64 count);
    void dataReceived(QString deviceMac, COLLECTOR_DataLoggerDataInfo_C *data, uint64 count);

private:
    // Функции обратного вызова
    static void cmdTransferCallbackStatic(char deviceMac[20], char *data, uint64 len);
    static void sensorInfoCallbackStatic(COLLECTOR_SensorInfo_C *info, uint64 count);
    static void dataCallbackStatic(COLLECTOR_DataLoggerDataInfo_C *data, uint64 count);

    void onCmdTransfer(QString deviceMac, QByteArray data);
    void onSensorInfo(const COLLECTOR_SensorInfo_C *info, uint64 count);
    void onDataReceived(const COLLECTOR_DataLoggerDataInfo_C *data, uint64 count);

private:
    QString m_deviceMac;
    QLibrary m_library;
    bool m_initialized;

    // Указатели на функции библиотеки
    typedef void (*InitAPIFunc)();
    typedef void (*ReleaseAPIFunc)();
    typedef void (*UpdateDeviceFunc)(COLLECTOR_DataLoggerInfo_C);
    typedef void (*StartContinuouslyCollectFunc)();
    typedef void (*StopCollectFunc)();
    typedef void (*StartSingleCollectFunc)();
    typedef void (*SetCollectIntervalFunc)(COLLECTOR_INTERVAL_C);
    typedef void (*GetSensorInfoFunc)();
    typedef void (*DemarcateSensorFunc)(COLLECTOR_DemarcateInfo_C);
    typedef void (*CalibrationSensorFunc)(COLLECTOR_CalibrationInfo_C);
    typedef void (*AppendDeviceDataFunc)(COLLECTOR_R_DATA_FLOW_C);

    typedef void (*RegisterCMDTransferCallFunc)(CMDTransferCallback);
    typedef void (*RegisterSensorUpdateCallFunc)(SensorInfoCallback);
    typedef void (*RegisterDataUpdateCallFunc)(DataLoggerDataCallback);

    InitAPIFunc m_initAPI;
    ReleaseAPIFunc m_releaseAPI;
    UpdateDeviceFunc m_updateDevice;
    StartContinuouslyCollectFunc m_startContinuouslyCollect;
    StopCollectFunc m_stopCollect;
    StartSingleCollectFunc m_startSingleCollect;
    SetCollectIntervalFunc m_setCollectInterval;
    GetSensorInfoFunc m_getSensorInfo;
    DemarcateSensorFunc m_demarcateSensor;
    CalibrationSensorFunc m_calibrationSensor;
    AppendDeviceDataFunc m_appendDeviceData;

    RegisterCMDTransferCallFunc m_registerCMDTransferCall;
    RegisterSensorUpdateCallFunc m_registerSensorUpdateCall;
    RegisterDataUpdateCallFunc m_registerDataUpdateCall;

    static QMap<QString, DeviceAPIHandler*> m_instances;
    static QMap<int32, DeviceAPIHandler*> m_macMap;
    static QMutex m_instancesMutex;
};

#endif // DEVICEAPIHANDLER_H
