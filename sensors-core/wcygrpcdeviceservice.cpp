#include "wcygrpcdeviceservice.h"
#include "wcygrpcwrapper.h"

WCYGRPCDeviceService::WCYGRPCDeviceService()
{
    _wcyAPI = std::make_unique<WCYServiceWrapper>(this);

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::messageReceived, [](const QString &msg) {
        qInfo().noquote() << "[INFO]" << msg;
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::errorOccurred, [](const QString &error) {
        qCritical().noquote() << "[ERROR]" << error;
    });

    QJsonArray unitsArray;

    // Загружаем units из JSON файла
    if (loadUnitsFromJson(unitsArray)) {
        // Успешно загружено из файла
        _units = unitsArray;
    } else {
        qWarning() << "Using default units data";
        _units.append(QJsonObject{{"id", 610},{"symbol", "N"},{"name", "Force"},{"koeff", 1},{"baseUnit", 13},{"availableUnits", QJsonArray{13}}});
        _units.append(QJsonObject{{"id", 700},{"symbol", "°"},{"name", "Angle"},{"koeff", 1},{"baseUnit", 14},{"availableUnits", QJsonArray{14}}});
        _units.append(QJsonObject{{"id", 660},{"symbol", "μs"},{"name", "Moment"},{"koeff", 1},{"baseUnit", 17},{"availableUnits", QJsonArray{17}}});
        _units.append(QJsonObject{{"id", 643},{"symbol", "mm"},{"name", "Distance"},{"koeff", 1},{"baseUnit", 18},{"availableUnits", QJsonArray{18}}});
        _units.append(QJsonObject{{"id", 661},{"symbol", "gA"},{"name", "Gate A"},{"koeff", 1},{"baseUnit", 19},{"availableUnits", QJsonArray{19}}});
        _units.append(QJsonObject{{"id", 662},{"symbol", "gB"},{"name", "Gate B"},{"koeff", 1},{"baseUnit", 20},{"availableUnits", QJsonArray{20}}});
    }
}

WCYGRPCDeviceService::~WCYGRPCDeviceService()
{
    qDebug() << "~WCYDeviceService";
    _connections.disconnectAll();
}

void WCYGRPCDeviceService::init()
{
    startSensorServer();
}

void WCYGRPCDeviceService::startScan()
{
    _currConnectType = USBCONNECT;
    if (_isServerConnected) {
        this->startDeviceScan(_currConnectType);
    }

    if (!_connections._serverConnect) {
        _connections._serverConnect = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::connectedChanged, [this](bool connected) mutable {
            if (!connected) {
                qCritical().noquote() << "[ERROR]" << "server is disconnected";
                _isServerConnected = false;
                return;
            }

            _isServerConnected = true;
            this->startDeviceScan(_currConnectType); // начинаем с usb, затем bluetooth
        });
    }

    if (!_connections._scanCompleted) {
        _connections._scanCompleted = connect(this, &WCYGRPCDeviceService::prescanCompleted, this, [this]() mutable {
            if (!_devices.isEmpty()) {
                connectNextDevice();
            }

            if (_currConnectType == USBCONNECT) {
                _currConnectType = CONNECT_TYPE::BLECONNECT;
                QTimer::singleShot(15000, this, [this]() {
                    this->startDeviceScan(_currConnectType);
                });
            }
        });
    }

    if (!_isServerConnected) {
        _wcyAPI->connectToServer("http://localhost:5001");
    }
}

void WCYGRPCDeviceService::startDeviceScan(CONNECT_TYPE connectType)
{
    // Лямбда-функция для преобразования DeviceModel в DeviceInfo
    auto deviceModelToDeviceInfo = [connectType](const DeviceModel& dev, int id) -> DeviceInfo {
        DeviceInfo dInfo;
        dInfo.id = id;
        dInfo.deviceId = dev.deviceId();
        dInfo.deviceMac = dev.deviceMac();
        dInfo.connectType = connectType;
        dInfo.connectState = CONNECT_STATE::StateDisconnected;

        QString devName = dev.deviceName();
        dInfo.deviceName = devName.trimmed();
        devName.remove(dev.deviceMac());

        dInfo.deviceType = devName;
        dInfo.displayType = devName;

        return dInfo;
    };

    if (connectType == CONNECT_TYPE::USBCONNECT) {
        _wcyAPI->scanUSB();
    } else if (connectType == CONNECT_TYPE::BLECONNECT){
        _wcyAPI->scanBLE(5000);
    }
    else if (connectType == CONNECT_TYPE::BTHCONNECT){
        _wcyAPI->scanBLE(5000);
    }

    connect(_wcyAPI.get(), &WCYServiceWrapper::devScanFinished, this,
        [this, deviceModelToDeviceInfo, connectType](const QList<DeviceModel> &devices, const QString &error) {
            qInfo().noquote() << "Found " << (connectType == CONNECT_TYPE::USBCONNECT ? "USB" : "BLE")
                              << " devices: " << devices.count();

            if (!error.isEmpty()) {
                qCritical() << "Scan error:" << error;
                return;
            }

            if (!devices.isEmpty()) {
                int id = _devices.size(); // Продолжаем нумерацию с текущего размера
                for(const auto& dev: devices) {
                    qInfo().noquote() << "[INFO]" << dev.deviceName() << "MAC: " << dev.deviceMac();
                    id++;

                    DeviceInfo dInfo = deviceModelToDeviceInfo(dev, id);

                    auto it = std::find_if(_devices.begin(), _devices.end(),
                                           [name = dInfo.deviceName](const DeviceInfo& device) {
                                               return device.deviceName == name;
                                           });

                    if (it != _devices.end()) continue; // устройство уже добавлено

                    _devices.append(dInfo);
                }
            }

            emit prescanCompleted();
        }, Qt::SingleShotConnection);
}

void WCYGRPCDeviceService::connectNextDevice()
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [](const DeviceInfo& device) {
                               return device.connectState == CONNECT_STATE::StateDisconnected || device.connectState == CONNECT_STATE::StateRemoved;
                           }
                           );

    if (it != _devices.end()) {
        connectToDevice(it->deviceMac);

        QTimer::singleShot(3000, this, [this]() {
            connectNextDevice();
        });
    } else {
        emit scanFinished(this);
        // Устройство не найдено
        return;
    }
}

void WCYGRPCDeviceService::connectToDevice(const QString& deviceMac)
{
    if (!_connections._deviceConnected) {
        _connections._deviceConnected = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::deviceConnected,
                                                         this, &WCYGRPCDeviceService::onDeviceConnected);
    }

    auto dInfo = getDeviceByMac(deviceMac);
    if (!dInfo) return;

    dInfo->connectState = CONNECT_STATE::StateConnecting;

    QTimer::singleShot(10000, this, [this, mac = dInfo->deviceMac]() {
        handleConnectionTimeout(mac);
    });

    _wcyAPI->connectDevice(dInfo->deviceMac);
}

void WCYGRPCDeviceService::handleConnectionTimeout(const QString& deviceMac)
{
    auto dInfo = getDeviceByMac(deviceMac);
    if (!dInfo) return;

    if (dInfo->connectState == CONNECT_STATE::StateConnecting) {
        qWarning() << "Connection timeout for device:" << deviceMac;
        // it->connectState = CONNECT_STATE::StateDisconnected;
        _devices.removeIf([&deviceMac](const DeviceInfo& device) {
            return device.deviceMac == deviceMac;
        });

        // connectNextDevice();
    }
}

void WCYGRPCDeviceService::deviceConnect(const QString &deviceMac)
{
    connectToDevice(deviceMac);
}

void WCYGRPCDeviceService::deviceDisconnect(const QString &deviceMac)
{
    _wcyAPI->disconnectDevice(deviceMac);
}

void WCYGRPCDeviceService::onSensorRangeReceived(const QString &deviceMac, QList<DataTypeModel> ranges)
{
    if (ranges.isEmpty()) {
        return;
    }

    auto dInfo = getDeviceByMac(deviceMac);
    if (!dInfo) return;

    int chInd = 0;
    for (const auto& range: ranges) {
        ChannelInfo& ch = dInfo->channels[chInd++];
        ch.id = range.rangeId();
        ch.name = range.rangeName();
        ch.symbol = range.unit();

        auto it = std::find_if(_units.begin(), _units.end(), [&range](const QJsonValue& unit) {
            return unit.toObject()["id"].toInt() == range.rangeId();
        });

        if (it != _units.end()) {
            QJsonObject unitObj = it->toObject();

            // Заполняем baseUnit
            ch.baseUnit = unitObj["baseUnit"].toInt();
            std::fill(std::begin(ch.availableUnits), std::end(ch.availableUnits), 0);

            // Заполняем availableUnits
            QJsonArray availableUnitsArray = unitObj["availableUnits"].toArray();
            int unitsCount = availableUnitsArray.size();

            for (int i = 0; i < unitsCount; ++i) {
                ch.availableUnits[i] = availableUnitsArray[i].toInt();
            }
        } else {
            // Если unit не найден, устанавливаем значения по умолчанию
            ch.baseUnit = 0;
            std::fill(std::begin(ch.availableUnits), std::end(ch.availableUnits), 0);
        }
    }

    dInfo->connectState = CONNECT_STATE::StateReady;
    _wcyAPI->subscribeToSensorEvents(dInfo->deviceMac);
    _wcyAPI->subscribeToSensorData(dInfo->deviceMac);

    emit updatedeviceStatus(this, *dInfo);

    if (!_connections._sensorEventReceived) {
        _connections._sensorEventReceived = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorEventReceived, [this](const QString &deviceMac, const QString &eventType, const QString &message) {
            if (eventType == "DeviceRemoved") {
                auto dInfo = getDeviceByMac(deviceMac);
                if (!dInfo) return;

                dInfo->connectState = CONNECT_STATE::StateRemoved;
                qDebug() << "Device removed successfully!" << dInfo->deviceMac;

                emit updatedeviceStatus(this, *dInfo);
            } else {
                qInfo() << "[INFO]: " << "device " << deviceMac << "event" << eventType << "message" << message;
            }
        });
    }

    if (!_connections._sensorDataReceived) {

        _connections._sensorDataReceived =  QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorDataReceived, [this](const QString& deviceMac, const QList<SensorDataModel> &sensorData) {

            if (sensorData.empty()) return;

            DataFlow dFlow;
            dFlow.timestamp = QTime::currentTime().msecsSinceStartOfDay() * 1000;
            dFlow.deviceMac = deviceMac;

            for (auto value: sensorData) {
                int chId = value.rangeId();
                dFlow.values[chId] = value.dataValue();
                //          qDebug() << "[STREAM]: " << "device mac" << dFlow.deviceMac << "ch id" << chId << ", value" << value.dataValue();
            }

            emit dataRecieve(dFlow);
        });
    }

    // connectNextDevice();
}

void WCYGRPCDeviceService::deviceStartStream(const QString &deviceMac)
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

    if (it == _devices.end()) {
        return;
    }

    DeviceInfo& dInfo = *it;
    QList<int32> ranges_idList;
    for (const auto& ch: dInfo.channels) {
        if (ch.id == 0) continue;
        ranges_idList.append(ch.id);
    }

    _wcyAPI->startSendData(dInfo.deviceMac, ranges_idList);

    connect(_wcyAPI.get(), &WCYServiceWrapper::startSendDataFinished, this,
        [this, deviceMac](bool success, const QString &error) {


            if (!error.isEmpty() || !success) {
                qWarning() << "Start send data failed!" << deviceMac;
                return;
            }

            auto dInfo = getDeviceByMac(deviceMac);
            if (!dInfo) return;

            dInfo->connectState = CONNECT_STATE::StateConnected;
            qDebug() << "Start send data successfully!" << dInfo->deviceMac;

            emit updatedeviceStatus(this, *dInfo);
        },  Qt::SingleShotConnection);
}

void WCYGRPCDeviceService::deviceStopStream(const QString &deviceMac)
{
    _wcyAPI->stopSendData(deviceMac);

    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

    if (it != _devices.end()) {
        DeviceInfo& dInfo = *it;
        dInfo.connectState = CONNECT_STATE::StateStopped;
        emit updatedeviceStatus(this, dInfo);
    }

    connect(_wcyAPI.get(), &WCYServiceWrapper::stopSendDataFinished, this,
        [this, deviceMac](bool success, const QString &error) {

            if (!error.isEmpty() || !success) {
                qWarning() << "Disconnect error, mac:" << deviceMac << "error: " << error;
                return;
            }

            auto dInfo = getDeviceByMac(deviceMac);
            if (!dInfo) return;

            dInfo->connectState = CONNECT_STATE::StateReady; // ?
            qDebug() << "Device disconnected successfully!" << dInfo->deviceMac;

            emit updatedeviceStatus(this, *dInfo);
        },  Qt::SingleShotConnection);
}

void WCYGRPCDeviceService::stopAll()
{
    for (const auto& dInfo: _devices) {
        if (dInfo.connectState == StateConnected) {
            _wcyAPI->stopSendData(dInfo.deviceMac);
            QTimer::singleShot(1000, this, [this, deviceMac = dInfo.deviceMac]() {
                   _wcyAPI->disconnectDevice(deviceMac);
            });
        } else if (dInfo.connectState == StateReady) {
            _wcyAPI->disconnectDevice(dInfo.deviceMac);
        }
    }
}

void WCYGRPCDeviceService::exit()
{
    _wcyAPI->disconnectFromServer();
    stopSensorServer();
}

const QList<DeviceInfo> WCYGRPCDeviceService::getConnectedDeviceList()
{
    QList<DeviceInfo> connectedDevices;

    std::copy_if(_devices.begin(), _devices.end(),
                 std::back_inserter(connectedDevices),
                 [](const DeviceInfo& device) {
                     return device.connectState == CONNECT_STATE::StateConnected
                        || device.connectState == CONNECT_STATE::StateReady
                        || device.connectState == CONNECT_STATE::StateStopped;
                 });

    return connectedDevices;
}

void WCYGRPCDeviceService::onDeviceConnected(const QString &deviceMac, ConnectEnumGadget::ConnectEnum result)
{
    if (!_connections._sensorRangeReceived) {
        _connections._sensorRangeReceived = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorRangeReceived,
                                                             this, &WCYGRPCDeviceService::onSensorRangeReceived);
    }

    if (result == ConnectEnumGadget::ConnectEnum::SUCCESSFUL) {
        auto dInfo = getDeviceByMac(deviceMac);
        if (dInfo != nullptr) {
            dInfo->connectState = CONNECT_STATE::StateConnecting;
            _wcyAPI->getSensorRange(deviceMac);
        }
    } else {
        _devices.removeIf([&deviceMac](const DeviceInfo& device) {
            return device.deviceMac == deviceMac;
        });
        // connectNextDevice(); // Продолжить со следующим устройством
    }
}

DeviceInfo* WCYGRPCDeviceService::getDeviceByMac(const QString &deviceMac)
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

    if (it == _devices.end()) {
        qWarning() << "Device not found:" << deviceMac;
        return nullptr;
    }

    return &(*it);
}

bool WCYGRPCDeviceService::loadUnitsFromJson(QJsonArray& _units) {
    // Получаем путь к директории с исполняемым файлом
    QString appDir = QCoreApplication::applicationDirPath();
    QString configPath = QDir(appDir).filePath("config/units_info.json");

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open config file:" << configPath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "Invalid JSON structure: root is not an object";
        return false;
    }

    QJsonObject root = doc.object();
    if (!root.contains("units") || !root["units"].isArray()) {
        qWarning() << "Invalid JSON structure: 'units' array not found";
        return false;
    }

    _units = root["units"].toArray();
    qDebug() << "Loaded" << _units.size() << "units from config";
    return true;
}

long WCYGRPCDeviceService::startSensorServer()
{
#ifndef QT_DEBUG
    stopProcessByNameDirect("WCYSensorRPCApi.exe");
#endif

    QString exePath = QCoreApplication::applicationDirPath() + "/WCY-SDK/WCYSensorRPCApi.exe";
    QFileInfo exeInfo(exePath);

    if (!exeInfo.exists()) {
        qDebug() << "Error: WCYSensorRPCApi.exe not found at" << exePath;
        return _return_FAIL;
    }

    qDebug() << "Starting WCYSensorRPCApi.exe from" << exePath;

    if (m_process.state() == QProcess::Running) {
        qDebug() << "Server WCYSensorRPCApi.exe is already running";
        return _return_OK;
    }

    m_process.setWorkingDirectory(QCoreApplication::applicationDirPath() + "/WCY-SDK");
    m_process.start(exePath);
    // m_process.startDetached(exePath);


    if (!m_process.waitForStarted(5000)) {
        qDebug() << "Error: Failed to start WCYSensorRPCApi.exe";
        qDebug() << "Error string:" << m_process.errorString();
        return _return_FAIL;
    }

    qDebug() << "WCYSensorRPCApi.exe started successfully";
    qDebug() << "Process PID:" << m_process.processId();

    // Подключаем обработчики для чтения вывода
    QObject::connect(&m_process, &QProcess::readyReadStandardOutput, [this]() {
        QString output = m_process.readAllStandardOutput();
        qDebug() << "WCYSensorRPCApi:" << output.trimmed();
    });

    QObject::connect(&m_process, &QProcess::readyReadStandardError, [this]() {
        QString error = m_process.readAllStandardError();
        qDebug() << "WCYSensorRPCApi ERROR:" << error.trimmed();
    });

    QObject::connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "WCYSensorRPCApi.exe finished with exit code:" << exitCode;

                     });

    return _return_OK;
}

long WCYGRPCDeviceService::stopSensorServer()
{

    // if (m_process.state() == QProcess::Running) {
    //     qDebug() << "Stopping WCYSensorRPCApi.exe...";
    //     m_process.terminate();


    //     if (!m_process.waitForFinished(3000)) {
    //         m_process.disconnect();
    //         qDebug() << "Force killing WCYSensorRPCApi.exe...";
    //         m_process.kill();
    //         m_process.waitForFinished(1000);
    //    }

    //     qDebug() << "WCYSensorRPCApi.exe stopped";

    //     return _return_OK;
    // }

    return _return_OK;
}

bool WCYGRPCDeviceService::stopProcessByNameDirect(const QString& processName)
{
    QProcess killProcess;
    killProcess.start("taskkill", QStringList() << "/IM" << processName << "/F");
    qDebug() << "Killing WCYSensorRPCApi.exe...";

    if (killProcess.waitForFinished(5000)) {
        qDebug() << "Process" << processName << "stopped";
        return true;
    } else {
        qDebug() << "Failed to kill process" << processName;
        return false;
    }
}
