// swrdeviceservice.cpp
#include "swrdeviceservice.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <algorithm>
#include <QRegularExpression>

SWRDeviceService::SWRDeviceService(QObject *parent)
    : IDeviceService()
    , m_bleManager(nullptr)
    , m_isInitialized(false)
    , m_isScanning(false)
    , m_scanTimer(nullptr)
{
    qRegisterMetaType<swr::dInfo>("swrdInfo");
    qRegisterMetaType<swr::R_DATA_FLOW>("R_DATA_FLOW");
}

SWRDeviceService::~SWRDeviceService()
{
    if (!m_isInitialized) return;

    cleanup();
}

void SWRDeviceService::init()
{
    if (m_isInitialized) {
        return;
    }

    // Инициализация Bluetooth менеджера
    initializeBluetooth();

    m_timer.start();
    m_isInitialized = true;

    qDebug() << "SWRDeviceService initialized successfully";
}

void SWRDeviceService::initializeBluetooth()
{
    m_bleManager = QBluetoothDeviceManagement::getBluetoothManagement();

    connect(m_bleManager, &QBluetoothDeviceManagement::updatedeviceStatus,
            this, &SWRDeviceService::onDeviceStatusUpdated, Qt::QueuedConnection);
    connect(m_bleManager, &QBluetoothDeviceManagement::dataRevice,
            this, &SWRDeviceService::onDataReceived, Qt::QueuedConnection);
    connect(m_bleManager, &QBluetoothDeviceManagement::scanFinished,
            this, &SWRDeviceService::onScanFinished, Qt::QueuedConnection);
}

DeviceAPIHandler* SWRDeviceService::createAPIHandler(const QString& deviceMac)
{
    auto* handler = new DeviceAPIHandler(deviceMac, this);

    connect(handler, &DeviceAPIHandler::cmdTransferRequired,
            this, &SWRDeviceService::onCmdTransferRequired);
    connect(handler, &DeviceAPIHandler::sensorInfoReceived,
            this, &SWRDeviceService::onSensorInfoReceivedFromAPI);
    connect(handler, &DeviceAPIHandler::dataReceived,
            this, &SWRDeviceService::onDataReceivedFromAPI);

    if (handler->initialize()) {
        return handler;
    } else {
        delete handler;
        return nullptr;
    }
}

void SWRDeviceService::startScan()
{
    if (!m_isInitialized) {
        qWarning() << "Service not initialized. Call init() first.";
        return;
    }

    if (m_isScanning) {
        qDebug() << "Scan already in progress";
        return;
    }

    // Проверяем, нет ли устройств в режиме стрима
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [](const DeviceInfo& dev) {
                               return dev.connectState == StateStarted;
                           });

    if (it != m_devices.end()) {
        qDebug() << "Device in streaming state, cannot start scan";
        emit scanFinished(this);
        return;
    }

    // Очищаем старые устройства, кроме подключенных
    QMutableMapIterator<QString, DeviceInfo> iter(m_devices);
    while (iter.hasNext()) {
        iter.next();
        if (iter.value().connectState != StateConnected &&
            iter.value().connectState != StateStarted) {
            iter.remove();
        }
    }

    m_isScanning = true;
    qDebug() << "Starting BLE scan...";
    m_bleManager->startScan();

    // Таймаут сканирования
    if (!m_scanTimer) {
        m_scanTimer = new QTimer(this);
        m_scanTimer->setSingleShot(true);
        connect(m_scanTimer, &QTimer::timeout, this, [this]() {
            if (m_isScanning) {
                m_bleManager->stopScan();
                m_isScanning = false;
                emit scanFinished(this);
            }
        });
    }
    m_scanTimer->start(10000);
}

void SWRDeviceService::stopScan()
{
    if (m_isScanning) {
        m_bleManager->stopScan();
        m_isScanning = false;
        if (m_scanTimer) {
            m_scanTimer->stop();
        }
    }
}

void SWRDeviceService::onDeviceStatusUpdated(swr::dInfo dInfo)
{
    QString deviceMac = QString::fromStdString(dInfo.deviceMac);
    QString deviceName = QString::fromStdString(dInfo.deviceName);

    // Фильтруем только нужные устройства
    QStringList nameFilters;
    nameFilters << "SD" << "W0" << "R0" << "TP";

    bool isTargetDevice = false;
    foreach (const QString& filter, nameFilters) {
        if (deviceName.startsWith(filter)) {
            isTargetDevice = true;
            break;
        }
    }

    if (!isTargetDevice) {
        return;
    }

    processScannedDevice(dInfo);
}

void SWRDeviceService::processScannedDevice(const swr::dInfo &dInfo)
{
    QMutexLocker locker(&m_devicesMutex);

    qDebug() << "Process scanned device:" << dInfo.deviceName;

    QString deviceMac = QString::fromStdString(dInfo.deviceMac);

    DeviceInfo* existingDevice = getDeviceByMac(deviceMac);

    if (!existingDevice) {
        // Новое устройство
        DeviceInfo newDevice = convertToDeviceInfo(dInfo);
        newDevice.id = m_devices.size() + 1;
        newDevice.connectState = StateScanned;
        m_devices.insert(deviceMac, newDevice);

        qInfo() << "New SWR device found:" << newDevice.deviceName
                << "MAC:" << deviceMac;

        // Создаем API handler для устройства, если ещё не создан
        DeviceAPIHandler* handler = nullptr;
        if (m_deviceAPIHandlers.contains(deviceMac)) {
            handler = m_deviceAPIHandlers[deviceMac];
        } else {
            handler = createAPIHandler(deviceMac);
            m_deviceAPIHandlers[deviceMac] = handler;
        }

        emit updatedeviceStatus(this, newDevice);
    } else {
        // Обновляем состояние существующего устройства
        CONNECT_STATE devState = existingDevice->connectState;

        if (dInfo.connectState == swr::dataLoggerStateConnected) {

            // Останавливаем таймер подключения если был
            if (m_connectionTimers.contains(deviceMac)) {
                m_connectionTimers[deviceMac]->stop();
                m_connectionTimers[deviceMac]->deleteLater();
                m_connectionTimers.remove(deviceMac);
            }

            DeviceAPIHandler* handler = m_deviceAPIHandlers.value(deviceMac,nullptr);
            if (!handler) return;

            existingDevice->connectState = StateConnected;
            // Обновляем информацию об устройстве в collector API
            updateDeviceInCollector(existingDevice);

            // QTimer::singleShot(200,this,[=](){
            //     updateDeviceInCollector(existingDevice);
            // });

            // Запрашиваем информацию о каналах
            QTimer::singleShot(2000,this,[=](){
                handler->getSensorInfo();
            });

        } else if (dInfo.connectState == swr::dataLoggerStateDisconnected) {
            m_bleManager->deviceDisConnect(deviceMac);
            existingDevice->connectState = StateDisconnected;
        }

        if (devState != existingDevice->connectState) {
            emit updatedeviceStatus(this, *existingDevice);
        }
    }
}

void SWRDeviceService::updateDeviceInCollector(const DeviceInfo* device)
{
    if (!device) return;

    COLLECTOR_DataLoggerInfo_C info;
    info.m_ConnectState = COLLECTOR_dataLoggerStateConnected;
    info.m_ConnectType = COLLECTOR_BLECONNECT;
    memset(info.m_Name, 0x00, 100);
    memset(info.m_MACAdress, 0x00, 20);

    QByteArray nameBa = device->deviceName.toUtf8();
    QByteArray macBa = device->deviceMac.toUtf8();

    memcpy(info.m_Name, nameBa.constData(), std::min(nameBa.size(), (qsizetype)99));
    memcpy(info.m_MACAdress, macBa.constData(), std::min(macBa.size(), (qsizetype)19));

    auto* handler = m_deviceAPIHandlers.value(device->deviceMac);
    if (handler) {
        handler->updateDevice(info);
    }
}

void SWRDeviceService::onScanFinished()
{
    m_isScanning = false;
    emit scanFinished(this);
}

void SWRDeviceService::onDeviceConnectionTimeout()
{
    QTimer* timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;

    QString deviceMac = timer->property("deviceMac").toString();
    timer->deleteLater();

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (device && device->connectState == StateConnecting) {
        qWarning() << "Connection timeout for device:" << deviceMac;
        m_bleManager->deviceDisConnect(deviceMac);
        device->connectState = StateDisconnected;

        m_deviceSensors[deviceMac].clear();

        emit updatedeviceStatus(this, *device);
    }
}

void SWRDeviceService::deviceConnect(const QString& deviceMac)
{
    qDebug() << "Connect to device" << deviceMac;

    if (!m_isInitialized) {
        qWarning() << "Service not initialized";
        return;
    }

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) {
        qWarning() << "Device not found:" << deviceMac;
        return;
    }

    if (device->connectState == StateConnected ||
        device->connectState == StateStarted) {
        qInfo() << "Device already connected:" << deviceMac;
        return;
    }

    device->connectState = StateConnecting;
    emit updatedeviceStatus(this, *device);

    // Таймер на подключение
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setProperty("deviceMac", deviceMac);
    connect(timeoutTimer, &QTimer::timeout, this, &SWRDeviceService::onDeviceConnectionTimeout);
    m_connectionTimers[deviceMac] = timeoutTimer;
    timeoutTimer->start(10000);

    m_bleManager->deviceConnect(deviceMac);
}

void SWRDeviceService::deviceDisconnect(const QString& deviceMac)
{
    QMutexLocker locker(&m_devicesMutex);

    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    if (device->connectState == StateStarted) {
       deviceStopInternal(device);
    }

    deviceDisconnectInternal(device);

    emit updatedeviceStatus(this, *device);
}

void SWRDeviceService::deviceDisconnectInternal(DeviceInfo* device)
{
    if (!device) return;
    QString deviceMac = device->deviceMac;

    m_bleManager->deviceDisConnect(deviceMac);

    device->connectState = StateDisconnected;
    m_streamingDevices.remove(deviceMac); // вообще-то вызывается в deviceStopStream

    m_deviceSensors.remove(deviceMac);
    m_lastTimestamps.remove(deviceMac);

    qInfo() << "Device disconnected:" << deviceMac;
}

void SWRDeviceService::deviceStartStream(const QString& deviceMac)
{
    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    if (device->connectState != StateConnected) {
        qWarning() << "Device not ready for streaming:" << deviceMac;
        return;
    }

    auto* handler = m_deviceAPIHandlers.value(deviceMac);
    if (!handler) {
        qWarning() << "No API handler for device:" << deviceMac;
        return;
    }

    // Начинаем непрерывный сбор данных
    handler->startContinuouslyCollect();

    device->connectState = StateStarted;
    m_streamingDevices.insert(deviceMac);

    // Инициализируем таймстемпы для каналов
    if (!m_lastTimestamps.contains(deviceMac)) {
        m_lastTimestamps[deviceMac] = QMap<int, qint64>();
    }

    emit updatedeviceStatus(this, *device);
    qInfo() << "Streaming started for device:" << deviceMac;

}

void SWRDeviceService::deviceStopStream(const QString& deviceMac)
{
    QMutexLocker locker(&m_devicesMutex);

    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    deviceStopInternal(device);
    emit updatedeviceStatus(this, *device);
}

void SWRDeviceService::deviceStopInternal(DeviceInfo* device)
{
    if (!device) return;

    if (device->connectState != StateStarted) {
        return;
    }

    QString deviceMac = device->deviceMac;
    auto* handler = m_deviceAPIHandlers.value(deviceMac);
    if (handler) {
        handler->stopCollect();
    }

    device->connectState = StateConnected;
    m_streamingDevices.remove(deviceMac);
    qInfo() << "Streaming stopped for device:" << deviceMac;
}

void SWRDeviceService::onDataReceived(swr::R_DATA_FLOW data)
{
    QString deviceMac = QString::fromStdString(data.dataLoggerInfo.deviceMac);

    auto* handler = m_deviceAPIHandlers.value(deviceMac);
    if (!handler) return;

    // Передаем сырые данные в collector API
    COLLECTOR_R_DATA_FLOW_C d;
    d.m_dataLoggerInfo.m_ConnectState = COLLECTOR_dataLoggerStateConnected;
    d.m_dataLoggerInfo.m_ConnectType = COLLECTOR_BLECONNECT;
    memset(d.m_dataLoggerInfo.m_Name, 0x00, 100);
    memset(d.m_dataLoggerInfo.m_MACAdress, 0x00, 20);

    QByteArray nameBa = QString::fromStdString(data.dataLoggerInfo.deviceName).toUtf8();
    QByteArray macBa = deviceMac.toUtf8();

    memcpy(d.m_dataLoggerInfo.m_Name, nameBa.constData(), std::min(nameBa.size(), (qsizetype)99));
    memcpy(d.m_dataLoggerInfo.m_MACAdress, macBa.constData(), std::min(macBa.size(), (qsizetype)19));

    d.m_dataPacket = reinterpret_cast<char*>(data.dataPacket.data());
    d.m_dataSize = data.dataPacket.size();

    handler->appendDeviceData(d);
}

void SWRDeviceService::onCmdTransferRequired(QString deviceMac, QByteArray data)
{
    sendDataToDevice(deviceMac, data);
}

void SWRDeviceService::onSensorInfoReceivedFromAPI(QString deviceMac, COLLECTOR_SensorInfo_C *info, uint64 count)
{
    if (!info || count == 0) return;
    updateDeviceChannels(deviceMac, info, count);
}

void SWRDeviceService::onDataReceivedFromAPI(QString deviceMac, COLLECTOR_DataLoggerDataInfo_C *data, uint64 count)
{
    if (!data || count == 0) return;
    processSensorData(deviceMac, data, count);
}

void SWRDeviceService::sendDataToDevice(const QString& deviceMac, const QByteArray& data)
{
    DeviceInfo* device = getDeviceByMac(deviceMac);
    // if (!device || device->connectState != StateStarted) {
    //     return;
    // }

    m_bleManager->sendData(deviceMac.toStdString(),
                           std::vector<unsigned char>(data.begin(), data.end()));
}

void SWRDeviceService::updateDeviceChannels(const QString& deviceMac,
                                            COLLECTOR_SensorInfo_C *sensorInfo,
                                            uint64 count)
{
    QMutexLocker locker(&m_devicesMutex);

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

     m_deviceSensors.remove(deviceMac); // Полностью удаляем старые данные перед обновлением

    // Очищаем старые каналы
    for (int i = 0; i < DeviceInfo::MAX_CHANNELS; i++) {
        device->channels[i] = ChannelInfo();
    }

    int channelIndex = 0;
    for (uint64 i = 0; i < count && channelIndex < DeviceInfo::MAX_CHANNELS; ++i) {
        COLLECTOR_SensorInfo_C& sensor = sensorInfo[i];

        // Сохраняем информацию о сенсоре
        if (sensor.m_HandleID > 7) continue;

        m_deviceSensors[deviceMac][sensor.m_HandleID] = sensor; // здесь не нужно глубокое копирование, так как работает побитовое копирование ив структуре нет указателей

        ChannelInfo& channel = device->channels[channelIndex++];
        channel.id = sensor.m_Channel;
        channel.name = QString::fromUtf8(sensor.m_Name);
        channel.symbol = QString::fromUtf8(sensor.m_Unit);
        channel.baseUnit = 1;
        channel.decimal = sensor.m_Decimal;
        channel.minValue = sensor.m_MinValue;
        channel.maxValue = sensor.m_MaxValue;

        // Заполняем доступные юниты
        channel.availableUnits[0] = 1; // Базовый юнит

        qDebug() << "Channel added for device" << deviceMac
                 << "ID:" << channel.id
                 << "HandleID:" << sensor.m_HandleID
                 << "Name:" << channel.name
                 << "Unit:" << channel.symbol
                 << "Decimal:" << sensor.m_Decimal
                 << "Range: [" << sensor.m_MinValue << "-" << sensor.m_MaxValue << "]";

    }

    // Обновляем информацию об устройстве
    emit updatedeviceStatus(this, *device);
}

void SWRDeviceService::processSensorData(const QString& deviceMac,
                                         COLLECTOR_DataLoggerDataInfo_C *dataInfo,
                                         uint64 count)
{
    if (!dataInfo || count == 0) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device || device->connectState != StateStarted) {
        return;
    }

    // Получаем или создаем статистику частоты для устройства
    checkDeviceFreq(device);

    // Получаем или создаем мапу таймстемпов для устройства
    if (!m_lastTimestamps.contains(deviceMac)) {
        m_lastTimestamps[deviceMac] = QMap<int, qint64>();
    }

    QMap<int, qint64>& deviceTimestamps = m_lastTimestamps[deviceMac];

    // Вычисляем шаг времени на основе частоты
    double time_step = 0;
    if (device->frequency > 0) {
        time_step = (1000000.0 / device->frequency); // микросекунды
    }

    qint64 baseTimestamp = QDateTime::currentMSecsSinceEpoch() * 1000;
    DataFlow dataFlow;

    for (uint64 i = 0; i < count; ++i) {
        COLLECTOR_DataLoggerDataInfo_C& data = dataInfo[i];
        int handleId = data.m_HandleID;

        // Проверяем, есть ли информация о сенсоре для этого handleId
        if (!m_deviceSensors[deviceMac].contains(handleId)) {
            continue;
        }

        qint64 currentTimestamp;
        if (!deviceTimestamps.contains(handleId)) {
            currentTimestamp = baseTimestamp;
            deviceTimestamps[handleId] = currentTimestamp;
        } else {
            currentTimestamp = deviceTimestamps[handleId] + static_cast<qint64>(time_step);
            deviceTimestamps[handleId] = currentTimestamp;
        }

        // Формируем DataFlow
        dataFlow.deviceMac = deviceMac;
        dataFlow.timestamp = currentTimestamp;
        dataFlow.values[handleId] = data.m_Value;

        // Логирование для отладки
        qInfo() << "Data from" << deviceMac
                << "Handle:" << handleId
                << "Value:" << data.m_Value
                << "Timestamp:" << currentTimestamp
                << "Freq:" << device->frequency;

    }
    emit dataRecieve(dataFlow);
}

void SWRDeviceService::checkDeviceFreq(DeviceInfo* device)
{
    if (!device) return;

    QString deviceMac = device->deviceMac;
    // Получаем или создаем статистику частоты для устройства
    if (!m_frequencyStats.contains(deviceMac)) {
        m_frequencyStats[deviceMac] = FrequencyStats();
    }

    FrequencyStats& stats = m_frequencyStats[deviceMac];

    // Обновляем статистику частоты
    stats.packetCount += 1;

    // Проверяем, прошла ли секунда с последнего обновления
    if (stats.measurementTimer.elapsed() >= m_frequencyUpdateInterval) {
        // Рассчитываем реальную частоту (пакетов в секунду)
        double elapsedSeconds = stats.measurementTimer.elapsed() / 1000.0;
        double realFrequency = stats.packetCount / elapsedSeconds;

        // Проверяем частоту устройства
        int freq = qRound(realFrequency);
        if (device->frequency != freq) {
            device->frequency = freq;

            qInfo() << "Frequency update for device:" << deviceMac
                    << "frequency:" << device->frequency << "Hz";

            qDebug() << "Frequency stats:"
                     << "Real frequency:" << realFrequency << "Hz"
                     << "Packets:" << stats.packetCount
                     << "Elapsed:" << elapsedSeconds << "s";
        }

        // Сбрасываем статистику
        stats.packetCount = 0;
        stats.measurementTimer.restart();
    }
}
void SWRDeviceService::stopAll()
{
    if (!m_isInitialized) return;

    QMutexLocker locker(&m_devicesMutex);

    // Останавливаем все устройства
    auto it = m_streamingDevices.begin();
    while (it != m_streamingDevices.end()) {
        QString deviceMac = *it;
        ++it;  // Инкремент перед вызовом, так как deviceStopStream может изменить контейнер
        deviceStopStream(deviceMac);
    }

    qDebug() << "All devices stopped";
}

void SWRDeviceService::exit()
{
    if (!m_isInitialized) return;

    cleanup();

    m_isInitialized = false;
    qDebug() << "SWRDeviceService exited";
}

void SWRDeviceService::cleanup()
{
    // Сохраняем список устройств для отключения
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        DeviceInfo* device = &it.value();
        if (device->connectState == StateStarted) {
            deviceStopInternal(device);
        }

        // if (device->connectState == StateConnected) {
        //     deviceDisconnectInternal(&it.value());
        // }
    }

    // Очищаем таймеры подключения
    // qDeleteAll(m_connectionTimers);
    for (auto key: m_connectionTimers.keys()) {
        auto t = m_connectionTimers[key];
        t->stop();
//      t->deleteLater(); ?
    }

    m_connectionTimers.clear();

    // Очищаем все API handlers
    for (const auto &key: m_deviceAPIHandlers.keys()) {
        auto handler = m_deviceAPIHandlers.take(key);
        handler->shutdown();
        handler->deleteLater();
    }
    m_deviceAPIHandlers.clear();

    if (m_bleManager) {
        m_bleManager->disconnect();
        m_bleManager->clearDevice();
        m_bleManager = nullptr;
    }

    m_devices.clear();
    m_deviceSensors.clear();
    m_streamingDevices.clear();
    m_lastTimestamps.clear();
    m_isInitialized = false;

}

const QList<DeviceInfo> SWRDeviceService::getConnectedDeviceList()
{
    QMutexLocker locker(&m_devicesMutex);

    QList<DeviceInfo> connectedDevices;
    for (const DeviceInfo& device : std::as_const(m_devices)) {
        if (device.connectState == StateScanned ||
            device.connectState == StateConnected ||
            device.connectState == StateDisconnected ||
            device.connectState == StateStarted ||
            device.connectState == StateConnecting) {
            connectedDevices.append(device);
        }
    }

    return connectedDevices;
}

DeviceInfo* SWRDeviceService::getDeviceByMac(const QString& deviceMac)
{
    auto it = m_devices.find(deviceMac);
    if (it != m_devices.end()) {
        return &it.value();
    }
    return nullptr;
}

DeviceInfo SWRDeviceService::convertToDeviceInfo(const swr::dInfo &dInfo)
{
    DeviceInfo newInfo;

    newInfo.deviceMac = QString::fromStdString(dInfo.deviceMac);
    newInfo.deviceName = QString::fromStdString(dInfo.deviceName);

    // Извлекаем тип устройства из имени
    QString name = newInfo.deviceName;
    QRegularExpression regex("([0-9A-Fa-f]{4})$");
    QRegularExpressionMatch match = regex.match(name);

    if (match.hasMatch()) {
        QString macNumber = match.captured(1);
        newInfo.deviceType = name.left(name.length() - macNumber.length()).trimmed();
        newInfo.deviceId = macNumber;
        newInfo.displayType = newInfo.deviceType + " - " + macNumber;
    } else {
        newInfo.deviceType = name;
        newInfo.displayType = name;
    }

    newInfo.connectType = BLECONNECT;

    if (dInfo.connectState == swr::dataLoggerStateConnected) {
        newInfo.connectState = StateConnected;
    } else {
        newInfo.connectState = StateScanned;
    }

    newInfo.frequency = 0; // По умолчанию 5 Гц

    return newInfo;
}

swr::dInfo SWRDeviceService::convertToSrcDeviceInfo(const DeviceInfo& deviceInfo)
{
    swr::dInfo dInfo;

    dInfo.deviceMac = deviceInfo.deviceMac.toStdString();
    dInfo.deviceName = deviceInfo.deviceName.toStdString();
    dInfo.deviceType = swr::BLECONNECT;

    if (deviceInfo.connectState == StateConnected ||
        deviceInfo.connectState == StateStarted) {
        dInfo.connectState = swr::dataLoggerStateConnected;
    } else {
        dInfo.connectState = swr::dataLoggerStateDisconnected;
    }

    return dInfo;
}
