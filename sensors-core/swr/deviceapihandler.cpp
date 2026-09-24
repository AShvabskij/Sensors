// deviceapihandler.cpp (исправленная версия)
#include "deviceapihandler.h"
#include <QDebug>
#include <QDir>

QMap<QString, DeviceAPIHandler*> DeviceAPIHandler::m_instances;
QMap<int32, DeviceAPIHandler*> DeviceAPIHandler::m_macMap;

QMutex DeviceAPIHandler::m_instancesMutex;

DeviceAPIHandler::DeviceAPIHandler(const QString& deviceMac, QObject *parent)
    : QObject(parent)
    , m_deviceMac(deviceMac)
    , m_initialized(false)
{
    QString libPath;
#ifdef Q_OS_WIN
    libPath = QDir::current().absoluteFilePath("collector_openinterface.dll");
#else
    libPath = QDir::current().absoluteFilePath("libcollector_openinterface.so");
#endif

    m_library.setFileName(libPath);

    // Регистрируем экземпляр
    QMutexLocker locker(&m_instancesMutex);
    m_instances[deviceMac] = this;
}

DeviceAPIHandler::~DeviceAPIHandler()
{
    shutdown();

    QMutexLocker locker(&m_instancesMutex);
    m_instances.remove(m_deviceMac);
}

bool DeviceAPIHandler::initialize()
{
    if (m_initialized) return true;

    if (!m_library.load()) {
        qCritical() << "Failed to load library:" << m_library.errorString();
        return false;
    }

    // Загружаем функции
    m_initAPI = (InitAPIFunc)m_library.resolve("initAPI");
    m_releaseAPI = (ReleaseAPIFunc)m_library.resolve("releaseAPI");
    m_updateDevice = (UpdateDeviceFunc)m_library.resolve("updateDevice");
    m_startContinuouslyCollect = (StartContinuouslyCollectFunc)m_library.resolve("startContinuouslyCollect");
    m_stopCollect = (StopCollectFunc)m_library.resolve("stopCollect");
    m_startSingleCollect = (StartSingleCollectFunc)m_library.resolve("startSingleCollect");
    m_setCollectInterval = (SetCollectIntervalFunc)m_library.resolve("setCollectInterval");
    m_getSensorInfo = (GetSensorInfoFunc)m_library.resolve("getSensorInfo");
    m_demarcateSensor = (DemarcateSensorFunc)m_library.resolve("DemarcateSensor");
    m_calibrationSensor = (CalibrationSensorFunc)m_library.resolve("CalibrationSensor");
    m_appendDeviceData = (AppendDeviceDataFunc)m_library.resolve("appendDeviceData");

    m_registerCMDTransferCall = (RegisterCMDTransferCallFunc)m_library.resolve("registerCMDTransferCall");
    m_registerSensorUpdateCall = (RegisterSensorUpdateCallFunc)m_library.resolve("registerSensorUpdateCall");
    m_registerDataUpdateCall = (RegisterDataUpdateCallFunc)m_library.resolve("registerDataUpdateCall");

    // Проверяем, что все функции загружены
    if (!m_initAPI || !m_releaseAPI || !m_updateDevice ||
        !m_startContinuouslyCollect || !m_stopCollect ||
        !m_registerCMDTransferCall || !m_registerSensorUpdateCall ||
        !m_registerDataUpdateCall) {
        qCritical() << "Failed to resolve required functions";
        return false;
    }

    // Инициализируем библиотеку
    m_initAPI();

    // Регистрируем callback'и для этого экземпляра
    m_registerCMDTransferCall(&DeviceAPIHandler::cmdTransferCallbackStatic);
    m_registerSensorUpdateCall(&DeviceAPIHandler::sensorInfoCallbackStatic);
    m_registerDataUpdateCall(&DeviceAPIHandler::dataCallbackStatic);

    m_initialized = true;
    qDebug() << "DeviceAPIHandler initialized for device:" << m_deviceMac;

    return true;
}

void DeviceAPIHandler::shutdown()
{
    if (m_initialized) {
        m_releaseAPI();
        m_initialized = false;
    }

    if (m_library.isLoaded()) {
        m_library.unload();
    }
}

void DeviceAPIHandler::updateDevice(const COLLECTOR_DataLoggerInfo_C& info)
{
    if (m_initialized && m_updateDevice) {
        qDebug() << "Update device for:" << info.m_MACAdress;
        m_updateDevice(info);
    }
}

void DeviceAPIHandler::startContinuouslyCollect()
{
    if (m_initialized && m_startContinuouslyCollect) {
        qDebug() << "Starting continuous collect for device:" << m_deviceMac;
        m_startContinuouslyCollect();
    }
}

void DeviceAPIHandler::stopCollect()
{
    if (m_initialized && m_stopCollect) {
        qDebug() << "Stopping collect for device:" << m_deviceMac;
        m_stopCollect();
    }
}

void DeviceAPIHandler::startSingleCollect()
{
    if (m_initialized && m_startSingleCollect) {
        m_startSingleCollect();
    }
}

void DeviceAPIHandler::setCollectInterval(COLLECTOR_INTERVAL_C interval)
{
    if (m_initialized && m_setCollectInterval) {
        m_setCollectInterval(interval);
    }
}

void DeviceAPIHandler::getSensorInfo()
{
    if (m_initialized && m_getSensorInfo) {
        qDebug() << "Getting sensor info for device:" << m_deviceMac;
        m_getSensorInfo();
    }
}

void DeviceAPIHandler::demarcateSensor(const COLLECTOR_DemarcateInfo_C& info)
{
    if (m_initialized && m_demarcateSensor) {
        m_demarcateSensor(info);
    }
}

void DeviceAPIHandler::calibrationSensor(const COLLECTOR_CalibrationInfo_C& info)
{
    if (m_initialized && m_calibrationSensor) {
        m_calibrationSensor(info);
    }
}

void DeviceAPIHandler::appendDeviceData(const COLLECTOR_R_DATA_FLOW_C& data)
{
    if (m_initialized && m_appendDeviceData) {
        m_appendDeviceData(data);
    }
}

// Статические callback'и
void DeviceAPIHandler::cmdTransferCallbackStatic(char deviceMac[20], char *data, uint64 len)
{
    QString macStr(deviceMac);
    QByteArray dataCopy(data, static_cast<int>(len));

    QMutexLocker locker(&m_instancesMutex);

    if (m_instances.contains(macStr)) {
        QMetaObject::invokeMethod(m_instances[macStr], [=]() {
            m_instances[macStr]->onCmdTransfer(macStr, dataCopy);
        }, Qt::QueuedConnection);
    }
}

void DeviceAPIHandler::sensorInfoCallbackStatic(COLLECTOR_SensorInfo_C *info, uint64 count)
{
    if (!info || count == 0) return;

    // Используем MAC адрес из первого сенсора для определения устройства
    QString deviceMac = QString::fromLocal8Bit(info[0].m_DeviceMac);
    int32 handleId = info[0].m_HandleID;

     QVector<COLLECTOR_SensorInfo_C> sensorsCopy = deepCopySensorInfoArray(info, count);

    QMutexLocker locker(&m_instancesMutex);
    if (m_instances.contains(deviceMac)) {
        m_macMap[handleId] = m_instances[deviceMac];
        QMetaObject::invokeMethod(m_instances[deviceMac], [=]() {
            m_instances[deviceMac]->onSensorInfo(sensorsCopy.data(), sensorsCopy.size());
        }, Qt::QueuedConnection);
    } else {
        qWarning() << "No handler found for device MAC:" << deviceMac;
    }
}

void DeviceAPIHandler::dataCallbackStatic(COLLECTOR_DataLoggerDataInfo_C *data, uint64 count)
{
    if (!data || count == 0) return;

    QVector<COLLECTOR_DataLoggerDataInfo_C> dataCopy = deepCopyDataInfoArray(data, count);
    QMutexLocker locker(&m_instancesMutex);
    int32 handleId = dataCopy.value(0).m_HandleID;

    if (m_macMap.contains(handleId)) {
        auto* handler = m_macMap[handleId];
        QMetaObject::invokeMethod(handler, [=]() {
            handler->onDataReceived(dataCopy.data(), dataCopy.size());
        }, Qt::QueuedConnection);
    }
}

void DeviceAPIHandler::onCmdTransfer(QString deviceMac, QByteArray data)
{
    emit cmdTransferRequired(m_deviceMac, data);
}

void DeviceAPIHandler::onSensorInfo(const COLLECTOR_SensorInfo_C *info, uint64 count)
{
    // Создаем копию данных для безопасной передачи
    QVector<COLLECTOR_SensorInfo_C> sensors;
    for (uint64 i = 0; i < count; ++i) {
        sensors.append(info[i]);
    }

    emit sensorInfoReceived(m_deviceMac, sensors.data(), sensors.size());
}

void DeviceAPIHandler::onDataReceived(const COLLECTOR_DataLoggerDataInfo_C *data, uint64 count)
{
    // Создаем копию данных для безопасной передачи
    QVector<COLLECTOR_DataLoggerDataInfo_C> dataList;
    for (uint64 i = 0; i < count; ++i) {
        dataList.append(data[i]);
    }

    emit dataReceived(m_deviceMac, dataList.data(), dataList.size());
}
