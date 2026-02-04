#include "wcydeviceservice.h"
#include "qthread.h"
#include <QDebug>
#include <algorithm>

WCYDeviceService::WCYDeviceService()
    : IDeviceService()
    , m_wrapper(nullptr)
    , m_isInitialized(false)
    , m_isScanning(false)
    , m_currentScanType(UNKNOWN)
{
    m_scanStatus.bleScanCompleted = false;
    m_scanStatus.hidScanCompleted = false;
}

WCYDeviceService::~WCYDeviceService()
{
}

void WCYDeviceService::init()
{
    if (m_isInitialized) {
        return;
    }

    m_wrapper = new WCY_DisLab_Wrapper(this);

    // Connect signals from wrapper
    connect(m_wrapper, &WCY_DisLab_Wrapper::deviceScanned,
            this, &WCYDeviceService::onDeviceScanned, Qt::DirectConnection);
    connect(m_wrapper, &WCY_DisLab_Wrapper::dataReceived,
            this, &WCYDeviceService::onDataReceived, Qt::QueuedConnection);
    connect(m_wrapper, &WCY_DisLab_Wrapper::deviceRemoved,
            this, &WCYDeviceService::onDeviceRemoved);
    connect(m_wrapper, &WCY_DisLab_Wrapper::errorOccurred,
            this, &WCYDeviceService::onErrorOccurred);

    // Initialize SDK
    m_isInitialized = m_wrapper->initialize();

    if (!m_isInitialized) {
        qCritical() << "Failed to initialize WCY SDK";
        return;
    }

    // Load units from JSON
    loadUnitsFromJson();

    m_timer.start();
    qDebug() << "WCYDeviceService initialized successfully";
}

void WCYDeviceService::startScan()
{
    if (!m_isInitialized) {
        qWarning() << "Service not initialized. Call init() first.";
        return;
    }

    if (m_isScanning) {
        qDebug() << "Scan already in progress";
        return;
    }

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [](const DeviceInfo& dev) {
                               return dev.connectState == StateStarted;
                           });

    if (it != m_devices.end()) {
        qDebug() << "There is a Device in starting data state, scanning is not allowed";
        m_isScanning = false;
        emit scanFinished(this);
        return;
    }

    QList<DeviceInfo> devices;
    for (const DeviceInfo& dInfo : m_devices) {
        if (dInfo.connectState == StateConnected || dInfo.connectState == StateStarted) {
            devices.append(dInfo); // оставим подключенные устройства не тронутыми
        }
    }
    m_devices = devices;

    m_isScanning = true;
    m_scanStatus.bleScanCompleted = false;
    m_scanStatus.hidScanCompleted = false;

    qDebug() << "Starting device scan...";

    // Start with HID (USB) scan first
    m_currentScanType = USBCONNECT;

    startHIDScan();
//            startBLEScan();
    // Start BLE scan after a delay
    QTimer::singleShot(1000, this, [this]() {
        m_currentScanType = BLECONNECT;
        m_scanStatus.bleScanCompleted = false;
        startBLEScan();
    });
}

void WCYDeviceService::stopScan()
{
    m_isScanning = false;
    qDebug() << "Scan stopped";
}

void WCYDeviceService::startBLEScan()
{
    if (!m_isScanning) return;

    qDebug() << "Starting BLE scan...";
    m_wrapper->startBLEScan(5); // 5 second scan timeout

    // Mark BLE scan as completed after timeout
    QTimer::singleShot(5000, this, [this]() {
        m_scanStatus.bleScanCompleted = true;
        qDebug() << "BLE scan completed";

        // Check if both scans are done
        if (m_scanStatus.bleScanCompleted && m_scanStatus.hidScanCompleted) {
            emit scanFinished(this);
            m_isScanning = false;
//          connectNextDevice();
        }
    });
}

void WCYDeviceService::startHIDScan()
{
    if (!m_isScanning) return;

    qDebug() << "Starting HID scan...";
    QVector<DeviceScan> hidDevices = m_wrapper->startHIDScan();

    // Process found HID devices
    for (const auto& device : hidDevices) {
        processScannedDevice(device, USBCONNECT);
    }

    m_scanStatus.hidScanCompleted = true;
    qDebug() << "HID scan completed, found" << hidDevices.size() << "devices";

    // Try to connect to found devices
    // if (!m_devices.isEmpty()) {
    //     connectNextDevice();
    // }

    // Check if both scans are done
    emit scanFinished(this);
    if (m_scanStatus.bleScanCompleted && m_scanStatus.hidScanCompleted) {
        m_isScanning = false;
    }
}

void WCYDeviceService::processScannedDevice(const DeviceScan& deviceScan, CONNECT_TYPE connectType)
{
    QMutexLocker locker(&m_devicesMutex);

    QString deviceMac = QString::fromLocal8Bit(deviceScan.macAddress);
    QString deviceName = QString::fromLocal8Bit(deviceScan.deviceName);

    // Check if device already exists
    DeviceInfo* devInfo = getDeviceByName(deviceName);

    if (devInfo == nullptr) {

        qInfo() << "[INFO] Scanned a new device:" << deviceName << "MAC:" << deviceMac
                << "Type:" << ConnectTypeToString(connectType);

        DeviceInfo newDevice = convertToDeviceInfo(deviceScan, connectType);
        newDevice.id = m_devices.size() + 1;
        newDevice.connectState = CONNECT_STATE::StateScanned;
        m_devices.append(newDevice);
    } else  if (devInfo->connectState == StateRemoved) {
        devInfo->connectState = CONNECT_STATE::StateScanned;
    } else {
        return;
    }


    // QTimer::singleShot(1000, [this, deviceMac]() {
    //     deviceConnect(deviceMac);
    // });
}

DeviceInfo WCYDeviceService::convertToDeviceInfo(const DeviceScan& deviceScan, CONNECT_TYPE connectType)
{
    DeviceInfo dInfo;
    dInfo.deviceMac = QString::fromLocal8Bit(deviceScan.macAddress);

    QString deviceName = QString::fromLocal8Bit(deviceScan.deviceName);

    // Разделяем имя устройства и идентификатор
    QString deviceType = deviceName;
    QString displaySuffix = "";

    // Ищем последние 4 символа, которые являются шестнадцатеричными цифрами
    QRegularExpression regex("([0-9A-Fa-f]{4})$");
    QRegularExpressionMatch match = regex.match(deviceName);

    QString devID = "";
    if (match.hasMatch()) {
        QString macNumber = match.captured(1);
        // Удаляем суффикс из имени устройства для deviceType
        deviceType = deviceName.left(deviceName.length() - macNumber.length());

        // Убираем возможный пробел в конце
        deviceType = deviceType.trimmed();

        // Формируем суффикс для displayType
        displaySuffix = " - " + macNumber;
        devID = macNumber;
    }

    dInfo.deviceId = devID;
    dInfo.deviceName = deviceName;
    dInfo.deviceType = deviceType;
    dInfo.displayType = deviceType + displaySuffix;
    dInfo.connectType = connectType;
    dInfo.connectState = StateUnknown;

    return dInfo;
}

DeviceScan WCYDeviceService::convertToDeviceScan(const DeviceInfo* device) const
{
    DeviceScan deviceScan = {};

    if (!device) {
        return deviceScan;
    }

    // Копируем имя устройства с использованием strncpy_s
    string deviceName = device->deviceName.toStdString();
    strncpy_s(deviceScan.deviceName, sizeof(deviceScan.deviceName),
              deviceName.c_str(), _TRUNCATE);

    // Копируем MAC адрес с использованием strncpy_s
    string macAddress = device->deviceMac.toStdString();
    strncpy_s(deviceScan.macAddress, sizeof(deviceScan.macAddress),
              macAddress.c_str(), _TRUNCATE);

    // Устанавливаем тип устройства
    deviceScan.deviceType = (device->connectType == USBCONNECT) ? 2 : 1; // 1=BLE, 2=HID

    return deviceScan;
}
void WCYDeviceService::deviceConnect(const QString& deviceMac)
{
    if (!m_isInitialized) {
        qWarning() << "Service not initialized";
        return;
    }

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) {
        qWarning() << "Device not found:" << deviceMac;
        return;
    }

    if (device->connectState == StateConnected || device->connectState == StateStarted) {
        qDebug() << "Device" << deviceMac << "is already connected";
        return;
    }

    auto ret = tryDeviceConnect(device);
    if (ret == _return_FAIL) {
        QTimer::singleShot(3000, [this, device]() {
            // another attempt
            tryDeviceConnect(device);
        });
    }
}

 _return_t WCYDeviceService::tryDeviceConnect(DeviceInfo* device)
{
     Q_ASSERT(device);

    QString deviceMac = device->deviceMac;
    device->connectState = StateConnecting;

//  handleConnectionTimeOut(deviceMac);

    // Connect using SDK
    DeviceScan deviceScan = convertToDeviceScan(device);
    bool connected = m_wrapper->connectDevice(deviceScan);

    if (!connected) {
        handleConnectionFailed(deviceMac);
        return _return_FAIL;
    }

    // Update device channels from SDK
    auto ret = updateDeviceChannels(device);
    if (ret != _return_OK) {
        handleConnectionFailed(deviceMac);
        return ret;
    }

    ret = setPreferedFrequency(deviceScan, 100);
    if (ret != _return_OK) {
        handleConnectionFailed(deviceMac);
        return ret;
    }

    QThread::msleep(300);

    int freq = m_wrapper->getCurrentFrequency(deviceScan);

    qInfo() << "Connection initiated for device:" << deviceMac << ", frequency = " << freq;
    device->connectState = StateConnected;
    device->frequency = freq;

    emit updatedeviceStatus(this, *device);

    return _return_OK;
}

_return_t WCYDeviceService::setPreferedFrequency(const DeviceScan& device, int frequencyHz)
{
    QVector<Frequency> freqList = m_wrapper->getSupportedFrequencies(device);

    if (freqList.isEmpty()) {
        return _return_FAIL;
    }

    QStringList freqStrings;
    for (const auto& freq : freqList) {
        freqStrings << QString::number(freq.frequency);
    }

    qDebug() << (QString("Supported frequencies: %1")
                   .arg(freqStrings.join(", ")));

    // Find closest frequency
    Frequency closestFreq = freqList.first();
    int minDifference = abs(freqList.first().frequency - frequencyHz);

    for (const auto& freq : freqList) {
        int diff = abs(freq.frequency - frequencyHz);
        if (diff < minDifference) {
            minDifference = diff;
            closestFreq = freq;
        }
    }

    qDebug() << QString("Set prefered frequency: %1 Hz").arg(closestFreq.frequency);


    bool bRes = m_wrapper->setFrequency(device, closestFreq.frequency);

    return bRes ? _return_OK : _return_FAIL;
}

void WCYDeviceService::deviceDisconnect(const QString& deviceMac)
{
    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    DeviceScan deviceScan = convertToDeviceScan(device);
    m_wrapper->disconnectDevice(deviceScan);

    auto predicate = [&deviceMac](const QPair<int, DeviceInfo*>& item) {
        return item.second->deviceMac == deviceMac;
    };

    m_ranges.removeIf(predicate);
    device->connectState = StateDisconnected;
    emit updatedeviceStatus(this, *device);

    qDebug() << "Device disconnected:" << deviceMac;
}

void WCYDeviceService::deviceStartStream(const QString& deviceMac)
{
    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    if (device->connectState != StateConnected) {
        qWarning() << "Device not ready for streaming:" << deviceMac;
        return;
    }

    auto ret = updateDeviceChannels(device);
    if (ret == _return_FAIL) {
        DeviceScan deviceScan = convertToDeviceScan(device);
        m_wrapper->disconnectDevice(deviceScan);
        ret = tryDeviceConnect(device);
    }
    if (ret == _return_FAIL) {
        return;
    }

    DeviceScan deviceScan = convertToDeviceScan(device);

    // Start data collection
    m_wrapper->startDataCollection(deviceScan);

    device->connectState = StateStarted;
    emit updatedeviceStatus(this, *device);

    qDebug() << "Data streaming started for device:" << deviceMac;
}

void WCYDeviceService::deviceStopStream(const QString& deviceMac)
{
    if (!m_isInitialized) return;

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    if (device->connectState != StateStarted) {
        qDebug() << "Device not streaming:" << deviceMac;
        return;
    }

//    DeviceScan deviceScan = convertToDeviceScan(device);

    // m_wrapper->stopDataCollection(deviceScan); // нельзя, так как стопит все остальные устройства, проблема на стороне SDK
    //  qDebug() << "Data streaming stopped for device:" << deviceMac;

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&device](const DeviceInfo& dev) {
                               return dev.connectState == StateStarted && dev.deviceMac != device->deviceMac;
                           });

    if (it == m_devices.end()) {
        stopStreamingAll();
    }

    device->connectState = StateConnected;
    emit updatedeviceStatus(this, *device);
}

void WCYDeviceService::stopStreamingAll()
{
    if (!m_isInitialized) return;

    QMutexLocker locker(&m_devicesMutex);

    for (auto& dev : m_devices) {

        if (dev.connectState == StateStarted) {
            DeviceScan deviceScan = convertToDeviceScan(&dev);
            m_wrapper->stopDataCollection(deviceScan);
        }
    }

    qDebug() << "All streaming stopped";
}

void WCYDeviceService::stopAll()
{
    if (!m_isInitialized) return;

    QMutexLocker locker(&m_devicesMutex);

    for (auto& device : m_devices) {

        DeviceScan deviceScan = convertToDeviceScan(&device);
        m_wrapper->stopDataCollection(deviceScan);
        m_wrapper->disconnectDevice(deviceScan);

        // if (device.connectState == StateStarted) {
        //     m_wrapper->stopDataCollection(deviceScan);
        //     device.connectState = StateConnected;
        // } else {

        //     if (device.connectState == StateConnected || device.connectState == StateConnecting) {
        //         m_wrapper->disconnectDevice(deviceScan);
        //         device.connectState = StateDisconnected;
        //     }
        // }
    }

    qDebug() << "All devices stopped";
}

void WCYDeviceService::exit()
{
    if (m_wrapper) {
        m_wrapper->dispose();
        m_wrapper->deleteLater();
        m_wrapper = nullptr;
    }

    m_isInitialized = false;
    m_isScanning = false;

    qDebug() << "WCYDeviceService exited";
}

const QList<DeviceInfo> WCYDeviceService::getConnectedDeviceList()
{
    QMutexLocker locker(&m_devicesMutex);

    QList<DeviceInfo> connectedDevices;
    std::copy_if(m_devices.begin(), m_devices.end(),
                 std::back_inserter(connectedDevices),
                 [](const DeviceInfo& device) {
                     return device.connectState == StateScanned || device.connectState == StateConnected  || device.connectState == StateDisconnected
                            || device.connectState == StateStarted || device.connectState == StateConnecting;
                 });

    return connectedDevices;
}

void WCYDeviceService::connectNextDevice()
{
    QMutexLocker locker(&m_devicesMutex);

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [](const DeviceInfo& device) {
                               return device.connectState == StateScanned;
                           });

    if (it != m_devices.end()) {
        deviceConnect(it->deviceMac);

        // Schedule next connection attempt
        QTimer::singleShot(3000, this, &WCYDeviceService::connectNextDevice);
    }
}

void WCYDeviceService::handleConnectionTimeOut(const QString& deviceMac)
{
    // Start connection timeout timer
    QTimer* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, [this, deviceMac, timer = timeoutTimer]() {
        timer->deleteLater();

        DeviceInfo* device = getDeviceByMac(deviceMac);
        if (!device) return;

        if (device->connectState == StateConnecting) {
            qWarning() << "Connection timeout for device:" << deviceMac;
            handleConnectionFailed(deviceMac);
        }
    });
    timeoutTimer->start(10000); // 10 second timeout
}

void WCYDeviceService::handleConnectionFailed(const QString& deviceMac)
{
    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    if (device->connectState == StateConnecting) {
        qWarning() << "Connection failed for device:" << deviceMac;

        // To disconnect the device just in case
        DeviceScan deviceScan = convertToDeviceScan(device);
        m_wrapper->disconnectDevice(deviceScan);

        device->connectState = StateRemoved;
        emit updatedeviceStatus(this, *device);
    }
}

DeviceInfo* WCYDeviceService::getDeviceByMac(const QString& deviceMac)
{
    QMutexLocker locker(&m_devicesMutex);

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&deviceMac](const DeviceInfo& dev) {
                               return dev.deviceMac == deviceMac;
                           });

    if (it == m_devices.end()) {
        qWarning() << "Device not found:" << deviceMac;
        return nullptr;
    }

    return &(*it);
}

DeviceInfo* WCYDeviceService::getDeviceByName(const QString& deviceName)
{
    QMutexLocker locker(&m_devicesMutex);

    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [&deviceName](const DeviceInfo& dev) {
                               return dev.deviceName == deviceName;
                           });

    if (it == m_devices.end()) {
        return nullptr;
    }

    return &(*it);
}

_return_t WCYDeviceService::updateDeviceChannels(DeviceInfo* device)
{
    Q_ASSERT(device);
    if (!device) return _return_FAIL;

    // Get data types (channels) from SDK
    DeviceScan deviceScan = convertToDeviceScan(device);
    QVector<ComboFetcher> fetchers = m_wrapper->getDataTypes(deviceScan);

    if (fetchers.isEmpty()) {
        qWarning() << "No channels found for device:" << device->deviceMac;
        return _return_FAIL;
    }

    // Update device channels
    int chIndex = 0;
    for (const auto& fetcher : fetchers) {
        if (chIndex >= DeviceInfo::MAX_CHANNELS) break;

        if (!m_ranges.contains(fetcher.rangeId)) {
            m_ranges.insert(fetcher.rangeId, device);
        }

        m_lastTimestamp[fetcher.rangeId] = 0;

        ChannelInfo& channel = device->channels[chIndex++];
        channel.id = fetcher.rangeId;
        channel.name = QString::fromLocal8Bit(fetcher.rangeName);

        // Find unit info
        auto unitIt = std::find_if(m_channelsDescr.begin(), m_channelsDescr.end(),
                                   [&fetcher](const QJsonValue& unit) {
                                       return unit.toObject()["id"].toInt() == fetcher.rangeId;
                                   });

        if (unitIt != m_channelsDescr.end()) {
            QJsonObject unitObj = unitIt->toObject();
            channel.symbol = unitObj["symbol"].toString();
            channel.baseUnit = unitObj["baseUnit"].toInt();

            QJsonArray availableUnits = unitObj["availableUnits"].toArray();
            int unitsCount = qMin(availableUnits.size(), ChannelInfo::MAX_UNITS);

            for (int i = 0; i < unitsCount; ++i) {
                channel.availableUnits[i] = availableUnits[i].toInt();
            }
        }
    }

    qDebug() << "Updated" << fetchers.size() << "channels for device:" << device->deviceMac;
    return _return_OK;
}

bool WCYDeviceService::loadUnitsFromJson()
{
    m_channelsDescr.append(QJsonObject{{"id", 1223},{"symbol", "N"},{"name", "Force"},{"koeff", 1},{"baseUnit", 13},{"availableUnits", QJsonArray{13}}});
    m_channelsDescr.append(QJsonObject{{"id", 1403},{"symbol", "°"},{"name", "Angle"},{"koeff", 1},{"baseUnit", 14},{"availableUnits", QJsonArray{14}}});
    m_channelsDescr.append(QJsonObject{{"id", 1323},{"symbol", "μs"},{"name", "Moment"},{"koeff", 1},{"baseUnit", 17},{"availableUnits", QJsonArray{17}}});
    m_channelsDescr.append(QJsonObject{{"id", 1289},{"symbol", "mm"},{"name", "Distance"},{"koeff", 1},{"baseUnit", 18},{"availableUnits", QJsonArray{18}}});
    m_channelsDescr.append(QJsonObject{{"id", 1325},{"symbol", "gA"},{"name", "Gate A"},{"koeff", 1},{"baseUnit", 19},{"availableUnits", QJsonArray{19}}});
    m_channelsDescr.append(QJsonObject{{"id", 1327},{"symbol", "gB"},{"name", "Gate B"},{"koeff", 1},{"baseUnit", 20},{"availableUnits", QJsonArray{20}}});

    // QString appDir = QCoreApplication::applicationDirPath();
    // QString configPath = QDir(appDir).filePath("config/units_info.json");

    // QFile file(configPath);
    // if (!file.open(QIODevice::ReadOnly)) {
    //     qWarning() << "Cannot open units config file:" << configPath;

    //     // Use default units as fallback
    //     m_channelsDescr.append(QJsonObject{{"id", 1223},{"symbol", "N"},{"name", "Force"},{"koeff", 1},{"baseUnit", 13},{"availableUnits", QJsonArray{13}}});
    //     m_channelsDescr.append(QJsonObject{{"id", 1403},{"symbol", "°"},{"name", "Angle"},{"koeff", 1},{"baseUnit", 14},{"availableUnits", QJsonArray{14}}});
    //     m_channelsDescr.append(QJsonObject{{"id", 660},{"symbol", "μs"},{"name", "Moment"},{"koeff", 1},{"baseUnit", 17},{"availableUnits", QJsonArray{17}}});
    //     m_channelsDescr.append(QJsonObject{{"id", 643},{"symbol", "mm"},{"name", "Distance"},{"koeff", 1},{"baseUnit", 18},{"availableUnits", QJsonArray{18}}});
    //     m_channelsDescr.append(QJsonObject{{"id", 661},{"symbol", "gA"},{"name", "Gate A"},{"koeff", 1},{"baseUnit", 19},{"availableUnits", QJsonArray{19}}});
    //     m_channelsDescr.append(QJsonObject{{"id", 662},{"symbol", "gB"},{"name", "Gate B"},{"koeff", 1},{"baseUnit", 20},{"availableUnits", QJsonArray{20}}});

    //     return false;
    // }

    // QByteArray data = file.readAll();
    // file.close();

    // QJsonParseError parseError;
    // QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    // if (parseError.error != QJsonParseError::NoError) {
    //     qWarning() << "JSON parse error:" << parseError.errorString();
    //     return false;
    // }

    // if (!doc.isObject()) {
    //     qWarning() << "Invalid JSON structure";
    //     return false;
    // }

    // QJsonObject root = doc.object();
    // if (!root.contains("units") || !root["units"].isArray()) {
    //     qWarning() << "'units' array not found in JSON";
    //     return false;
    // }

    // m_channelsDescr = root["units"].toArray();
    // qDebug() << "Loaded" << m_channelsDescr.size() << "units from config";

    return true;
}

void WCYDeviceService::onDeviceScanned(const DeviceScan& deviceScan)
{
    processScannedDevice(deviceScan, BLECONNECT);
}

void WCYDeviceService::onDataReceived(const WCY_DisLab_Wrapper::RData& data)
{
    auto device = m_ranges.value(data.rangeId, nullptr);
    if (device == nullptr) return;

    QString deviceMac = device->deviceMac;

    // auto device = getDeviceByMac(deviceMac);
    if (device->connectState != StateStarted) return;

    // if (m_lastTimestamp[data.rangeId] == 0) {
    //     m_lastTimestamp[data.rangeId] = data.timestamp;
    // }

    DataFlow dFlow;
    double time_step = (1.0 / device->frequency) * 1000 * 1000;
    dFlow.timestamp = m_lastTimestamp[data.rangeId] + static_cast<int64>(time_step);
    dFlow.deviceMac = deviceMac;
    dFlow.values[data.rangeId] = data.value;

    m_lastTimestamp[data.rangeId] = dFlow.timestamp;

    // if (m_lastTimestamp[data.rangeId] - data.timestamp > 1000*100) {
    //     m_lastTimestamp[data.rangeId] = data.timestamp;
    // }

//    qInfo() << "timestamp: " << dFlow.timestamp;
    emit dataRecieve(dFlow);
}

void WCYDeviceService::onDeviceRemoved(const DeviceScan& deviceScan)
{
    QString deviceMac = QString::fromLocal8Bit(deviceScan.macAddress);

    DeviceInfo* device = getDeviceByMac(deviceMac);
    if (!device) return;

    device->connectState = StateRemoved;
    emit updatedeviceStatus(this, *device);

    qDebug() << "Device removed:" << deviceMac;
}

void WCYDeviceService::onErrorOccurred(const QString& errorMessage)
{
    qCritical() << "[ERROR]" << errorMessage;
}
