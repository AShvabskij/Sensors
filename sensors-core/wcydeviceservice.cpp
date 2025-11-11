#include "wcydeviceservice.h"


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
    _wcyAPI->disconnectFromServer();
}

void WCYDeviceService::startScan()
{
//    QObject::connect(_wcyAPI.get(), SIGNAL(connectedChanged(bool)), this, SLOT(onConnected(bool)), Qt::UniqueConnection);

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::connectedChanged, [this](bool connected) {
        if (connected) {
            //  client->scanBLE(5000); // 5 секунд сканирования
            _wcyAPI->scanUSB();
        } else {
            qCritical().noquote() << "[ERROR]" << "server is disconnected";
        }
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::devScanFinished, [this](const QList<DeviceModel> &devices, const QString &error) {
        qInfo().noquote() << "Found devices: " << devices.count();

        if (!_devices.isEmpty() || devices.count() == 0) {
            return;
        }

        int id = 0;
        for(const auto& dev: devices) {
            qInfo().noquote() << "[INFO]" << dev.deviceName() << "MAC: " << dev.deviceMac();
            id++;
            DeviceInfo dInfo;
            dInfo.id = id;
            dInfo.deviceId = dev.deviceId();
            dInfo.deviceMac = dev.deviceMac();
            dInfo.connectType = CONNECT_TYPE::USBCONNECT;
            dInfo.connectState = CONNECT_STATE::StateConnected;

            QString devName = dev.deviceName();
            devName.remove(dev.deviceMac());
            dInfo.deviceName = devName;
            dInfo.deviceType = "WCY_SENSOR_"+ devName;
            dInfo.displayType = devName;

            _devices.append(dInfo);

            _wcyAPI->connectDevice(dInfo.deviceMac);
        }

        emit scanFinished(this);
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::deviceConnected,
                     [this](const QString &mac, ConnectEnumGadget::ConnectEnum result) {
                         if (result == ConnectEnumGadget::ConnectEnum::SUCCESSFUL) {
                             _wcyAPI->getSensorRange(mac);
                         }
                     });


    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorRangeReceived,
                     [this](const QString &mac, QList<DataTypeModel> ranges) {
                         auto it = std::find_if(_devices.begin(), _devices.end(),
                                                [&mac](const DeviceInfo& dev) { return dev.deviceMac == mac.toStdString(); });

                         if (it != _devices.end()) {
                             DeviceInfo& connectedDevice = *it;
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
                         }
                     });

    _wcyAPI->connectToServer("http://localhost:5001");
}

void WCYDeviceService::deviceStartStream(const QString &deviceMac)
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
    _wcyAPI->subscribeToSensorData(dInfo.deviceMac);
    _wcyAPI->subscribeToSensorEvents(dInfo.deviceMac);

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorDataReceived, [this, dInfo = std::move(dInfo)](const QList<SensorDataModel> &sensorData) {

        if (sensorData.empty()) return;
        qInfo() << "[STREAM]: " << "range id" << sensorData.first().rangeId() << ", value" << sensorData.first().dataValue();

        DataFlow dFlow;
        dFlow.timestamp = QTime::currentTime().msecsSinceStartOfDay() * 1000;
        dFlow.deviceId = dInfo.id;
        dFlow.deviceMac = dInfo.deviceMac;

        for (auto value: sensorData) {
            int chId = value.rangeId();
            // todo: check if dInfo contains channel with id = chId
            dFlow.values[chId] = value.dataValue();
        }

        emit dataRecieve(dFlow);
    });

    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::sensorEventReceived, [](const QString &deviceMac, const QString &eventType, const QString &message) {
        qInfo() << "[INFO]: " << "device " << deviceMac << "event" << eventType << "message" << message;
    });
}

void WCYDeviceService::deviceDisconnect(const QString &deviceMac)
{
    _wcyAPI->stopSendData(deviceMac);
    QObject::connect(_wcyAPI.get(), &WCYServiceWrapper::stopSendDataFinished,
                     [this, deviceMac](bool success, const QString &error) {
                        _wcyAPI->deviceExit(deviceMac);
                     });

}

void WCYDeviceService::closeAll()
{
    for (const auto& dInfo: _devices) {
        _wcyAPI->stopSendData(dInfo.deviceMac);
        _wcyAPI->deviceExit(dInfo.deviceMac);
    }
}

void WCYDeviceService::onConnected(bool connected) {
    if (connected) {
        //  client->scanBLE(5000); // 5 секунд сканирования
        _wcyAPI->scanUSB();
    } else {
        qCritical() << "[ERROR]" << "server is disconnected";
    }
}
