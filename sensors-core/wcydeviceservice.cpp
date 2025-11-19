#include "wcydeviceservice.h"
#include "wcygrpcwrapper.h"

WCYDeviceService::WCYDeviceService()
{
    _wcyAPI = std::make_unique<WCYServiceWrapper>(this);

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::messageReceived, [](const QString &msg) {
        qInfo().noquote() << "[INFO]" << msg;
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::errorOccurred, [](const QString &error) {
        qCritical().noquote() << "[ERROR]" << error;
    });

}

WCYDeviceService::~WCYDeviceService()
{
    qDebug() << "~WCYDeviceService";
    _connections.disconnectAll();

}

void WCYDeviceService::startScan()
{
    if (!_connections._serverConnect) {
        _connections._serverConnect = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::connectedChanged, [this](bool connected) {
            if (connected) {
                this->startDeviceScan(CONNECT_TYPE::USBCONNECT); // начинаем с usb, затем bluetooth
            } else {
                qCritical().noquote() << "[ERROR]" << "server is disconnected";
            }
        });
    }

    connect(this, &WCYDeviceService::usbScanCompleted, this, [this]() {
            QTimer::singleShot(3000, this, [this]() {
                this->startDeviceScan(CONNECT_TYPE::BLECONNECT);
            });
        }, Qt::SingleShotConnection);

    // connect(this, &WCYDeviceService::bleScanUncompleted, this, [this]() {
    //         QTimer::singleShot(5000, this, [this]() {
    //             this->startDeviceScan(CONNECT_TYPE::BLECONNECT);
    //         });
    //     }, Qt::SingleShotConnection);

    _wcyAPI->connectToServer("http://localhost:5001");
}

void WCYDeviceService::startDeviceScan(CONNECT_TYPE connectType)
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

        dInfo.deviceType = "WCY_SENSOR_" + devName;
        dInfo.displayType = devName;

        return dInfo;
    };

    if (connectType == CONNECT_TYPE::USBCONNECT) {
        _wcyAPI->scanUSB();
    } else if (connectType == CONNECT_TYPE::BLECONNECT){
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
            } else {
                if (connectType == CONNECT_TYPE::BLECONNECT) {
                    emit bleScanUncompleted();
                }
            }

            // Определяем что делать после завершения сканирования
            if (connectType == CONNECT_TYPE::USBCONNECT) {
                emit usbScanCompleted();
            } else {
                connectNextDevice();
            }
        }, Qt::SingleShotConnection);
}

void WCYDeviceService::connectNextDevice()
{
    if (!_connections._deviceConnected) {
        _connections._deviceConnected = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::deviceConnected,
                                            this, &WCYDeviceService::onDeviceConnected);
    }

    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [](const DeviceInfo& device) {
                               return device.connectState == CONNECT_STATE::StateDisconnected;
                           }
                           );

    if (it != _devices.end()) {
        DeviceInfo& dInfo = *it;
        dInfo.connectState = CONNECT_STATE::StateConnecting;

        QTimer::singleShot(10000, this, [this, mac = dInfo.deviceMac]() {
            handleConnectionTimeout(mac);
        });

        _wcyAPI->connectDevice(dInfo.deviceMac);
    } else {
        emit scanFinished(this);
        // Устройство не найдено
        return;
    }
}

void WCYDeviceService::handleConnectionTimeout(const QString& deviceMac)
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) {
                               return dev.deviceMac == deviceMac;
                           });

    if (it != _devices.end() && it->connectState == CONNECT_STATE::StateConnecting) {
        qWarning() << "Connection timeout for device:" << deviceMac;
        it->connectState = CONNECT_STATE::StateDisconnected;
        connectNextDevice();
    }
}

void WCYDeviceService::deviceConnect(const QString &deviceMac)
{
    if (!_connections._sensorRangeReceived) {
        _connections._sensorRangeReceived = QObject::connect(_wcyAPI.get(),
                                                             &WCYServiceWrapper::sensorRangeReceived, this, &WCYDeviceService::onSensorRangeReceived);
    }

    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

    if (it == _devices.end()) {
        qWarning() << "Device not found:" << deviceMac;
        return;
    }

    if (it->connectState != CONNECT_STATE::StateConnecting) {
        qWarning() << "Device is not in connecting state:" << deviceMac;
        return;
    }

    _wcyAPI->getSensorRange(it->deviceMac);
}

void WCYDeviceService::onSensorRangeReceived(const QString &mac, QList<DataTypeModel> ranges)
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&mac](const DeviceInfo& dev) { return dev.deviceMac == mac.toStdString(); });

    if (it != _devices.end()) {
        DeviceInfo& connectedDevice = *it;
        connectedDevice.connectState = CONNECT_STATE::StateReady;
        int chInd = 0;
        for (const auto& range: ranges) {
            ChannelInfo& ch = connectedDevice.channels[chInd++];
            ch.id = range.rangeId();
            ch.name = range.rangeName();
            ch.symbol = range.unit();
            ch.baseUnit = range.rangeId();
            ch.availableUnits[0] = ch.baseUnit;
        }
        emit updatedeviceStatus(this, connectedDevice);
        //                           deviceStartStream(mac);
    }

    connectNextDevice();
}

void WCYDeviceService::deviceStartStream(const QString &deviceMac)
{
    auto it = std::find_if(_devices.begin(), _devices.end(),
                           [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

    if (it == _devices.end()) {
        return;
    }

    DeviceInfo& dInfo = *it;
    if (dInfo.connectState != StateConnected) {
        dInfo.connectState = StateConnected;
        emit updatedeviceStatus(this, dInfo);
    }

    QList<int32> ranges_idList;
    for (const auto& ch: dInfo.channels) {
        if (ch.id == 0) continue;
        ranges_idList.append(ch.id);
    }

    _wcyAPI->startSendData(dInfo.deviceMac, ranges_idList);
    _wcyAPI->subscribeToSensorData(dInfo.deviceMac);
    _wcyAPI->subscribeToSensorEvents(dInfo.deviceMac);

    if (_connections._sensorDataReceived && _connections._sensorEventReceived) return;

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

    _connections._sensorEventReceived = QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorEventReceived, [](const QString &deviceMac, const QString &eventType, const QString &message) {
        qInfo() << "[INFO]: " << "device " << deviceMac << "event" << eventType << "message" << message;
    });
}

void WCYDeviceService::deviceDisconnect(const QString &deviceMac)
{
    _wcyAPI->stopSendData(deviceMac);
    connect(_wcyAPI.get(), &WCYServiceWrapper::stopSendDataFinished, this,
                     [this, deviceMac](bool success, const QString &error) {

            if (!error.isEmpty() || !success) {
                qWarning() << "Disconnect error, mac:" << deviceMac << "error: " << error;
                return;
            }

            auto it = std::find_if(_devices.begin(), _devices.end(),
                                   [&deviceMac](const DeviceInfo& dev) { return dev.deviceMac == deviceMac.toStdString(); });

            if (it == _devices.end()) {
                qWarning() << "Device not found:" << deviceMac;
                return;
            }

            DeviceInfo& dInfo = *it;
            dInfo.connectState = CONNECT_STATE::StateReady; // ?
            qDebug() << "Device disconnected successfully!" << dInfo.deviceMac;

            emit updatedeviceStatus(this, dInfo);
        },  Qt::SingleShotConnection);
}

void WCYDeviceService::stopAll()
{
    for (const auto& dInfo: _devices) {
        if (dInfo.connectState == StateConnected || dInfo.connectState == StateReady) {
            _wcyAPI->stopSendData(dInfo.deviceMac);
            QTimer::singleShot(1000, this, [this, deviceMac = dInfo.deviceMac]() {
                   _wcyAPI->disconnectDevice(deviceMac);
            });
        } /*else if (dInfo.connectState == StateReady) {
            _wcyAPI->disconnectDevice(dInfo.deviceMac);
        }*/

//      _wcyAPI->deviceExit(dInfo.deviceMac);
    }
}

void WCYDeviceService::exit()
{
    _wcyAPI->disconnectFromServer();
}

const QList<DeviceInfo> WCYDeviceService::getConnectedDeviceList()
{
    QList<DeviceInfo> connectedDevices;

    std::copy_if(_devices.begin(), _devices.end(),
                 std::back_inserter(connectedDevices),
                 [](const DeviceInfo& device) {
                     return device.connectState == CONNECT_STATE::StateConnected || device.connectState == CONNECT_STATE::StateReady;
                 });

    return connectedDevices;
}

void WCYDeviceService::onDeviceConnected(const QString &deviceMac, ConnectEnumGadget::ConnectEnum result)
{
    if (result == ConnectEnumGadget::ConnectEnum::SUCCESSFUL) {
        auto it = std::find_if(_devices.begin(), _devices.end(),
                               [&deviceMac](const DeviceInfo& dev) {
                                   return dev.deviceMac == deviceMac.toStdString();
                               });
        if (it != _devices.end()) {
            DeviceInfo& dInfo = *it;
            dInfo.connectState = CONNECT_STATE::StateConnecting;
            deviceConnect(deviceMac);
        }
    } else {
        _devices.removeIf([&deviceMac](const DeviceInfo& device) {
            return device.deviceMac == deviceMac;
        });
        connectNextDevice(); // Продолжить с следующим устройством
    }
}
