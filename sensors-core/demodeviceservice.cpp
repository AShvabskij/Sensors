#include "demodeviceservice.h"

#include <QtCore>
#include <QtConcurrent/QtConcurrent>
#include "device_generator.h"

DemoDeviceService::DemoDeviceService() {}

void DemoDeviceService::startScan()
{
    if (m_devices.empty()) {
        generateDevicesUpdate(); // fill m_devices
    }
    emit scanFinished(this);
}

void DemoDeviceService::deviceStartStream(const QString &deviceMac)
{
    DeviceInfo dInfo;

    for (int i = 0; i < m_devices.count(); i++) {
        QJsonObject jsndInfo = m_devices[i].toObject();
        if (jsndInfo["address"] == deviceMac) {
            jsndInfo["connected"] = true;
            jsndInfo["process"] = true;

            dInfo = convertToDeviceInfo(jsndInfo);
            m_devices[i] = jsndInfo;
            break;
        }
    }

    if (dInfo.deviceId.isEmpty()) return;

#ifdef __linux__
    if (!m_threadFuture.isRunning()) {
        m_threadFuture = QtConcurrent::run(&DemoDeviceService::thread_proc, this);
    }
#else
    if (!m_threadFuture.isRunning()) {
        m_threadFuture = QtConcurrent::run(&DemoDeviceService::thread_proc, this);
    }
#endif

    emit updatedeviceStatus(this, dInfo);
    m_dataEnabled = true;
    return;
}

void DemoDeviceService::deviceStopStream(const QString &deviceMac)
{
    DeviceInfo dInfo;

    bool isDataStarted = false;
    for (int i = 0; i < m_devices.count(); i++) {
        QJsonObject jsndInfo = m_devices[i].toObject();
        if (jsndInfo["address"] == deviceMac) {
            jsndInfo["connected"] = false;

            dInfo = convertToDeviceInfo(jsndInfo);
            m_devices[i] = jsndInfo;
        }
        if (jsndInfo["process"] == true) { // if any device is started yet
            isDataStarted = true;
        }
    }

    m_dataEnabled = isDataStarted;

    if (dInfo.deviceId.isEmpty()) return;
    emit updatedeviceStatus(this, dInfo);

}

void DemoDeviceService::stopAll()
{
    m_dataEnabled = false;
}

void DemoDeviceService::exit()
{
    if (m_threadFuture.isRunning() || m_threadFuture.isStarted()) {
        m_threadFuture.cancel();
        m_threadFuture.waitForFinished();
    }
}

const QList<DeviceInfo> DemoDeviceService::getConnectedDeviceList()
{
    QList<DeviceInfo> connectedDevices;

    for (auto jsdInfo: m_devices) {
        DeviceInfo dInfo = convertToDeviceInfo(jsdInfo.toObject());
        if (dInfo.connectState == CONNECT_STATE::StateConnected
            || dInfo.connectState == CONNECT_STATE::StateConnecting
            || dInfo.connectState == CONNECT_STATE::StateStarted
            || dInfo.connectState == CONNECT_STATE::StateDisconnected
            ) {

            connectedDevices.append(dInfo);
        }
    }

    return connectedDevices;
}

DeviceInfo DemoDeviceService::convertToDeviceInfo(const QJsonObject& sensor)
{
    DeviceInfo res;
    res.deviceId = sensor["id"].toString();
    res.deviceMac = sensor["address"].toString();
    res.deviceType = sensor["type"].toString();
    res.deviceName = sensor["name"].toString();
    res.displayType = sensor["displayType"].toString();
    res.connectState = sensor["connected"].toBool() == true ? StateStarted : StateConnected;

    int ch_ind = 0;
    auto channels = sensor["channels"].toArray();
    for (auto ch: channels) {
        QJsonObject jsnChannel = ch.toObject();
        ChannelInfo chInfo;
        chInfo.id = jsnChannel["id"].toInt();
        chInfo.name = jsnChannel["name"].toString();
        chInfo.symbol = jsnChannel["symbol"].toString();
        chInfo.baseUnit = jsnChannel["unit"].toInt();

        int i = 0;
        for (auto unit: jsnChannel["availableUnits"].toArray()) {
            int unitId = unit.toInt();
            chInfo.availableUnits[i++] = unitId;
        }

        res.channels[ch_ind++] = chInfo;
    }

    return res;
}

QJsonObject DemoDeviceService::generateDevicesUpdate() {

    QJsonObject root;
    root["type"] = "devices_update";

    m_devices = QJsonArray();
    // Вольтметр
    QJsonArray voltUnits;
    voltUnits << 2 << 3;
    QJsonArray voltChannels;
    voltChannels.append(DeviceGenerator::createChannel(1, "U", "voltage", 3, voltUnits));

    m_devices.append(DeviceGenerator::createSensor(
        "38:3B:26:E8:A3:3A", "voltmeter", "A3A3", "VM-1001_A3A3",
        "вольтметр", "USB", false, voltChannels
        ));

    // Амперметр
    QJsonArray ampUnits;
    ampUnits << 0 << 1 << 7;
    QJsonArray ampChannels;
    ampChannels.append(DeviceGenerator::createChannel(18, "f", "f", 1, ampUnits));

    m_devices.append(DeviceGenerator::createSensor(
        "38:3B:26:E8:A3:3D", "ampermeter", "3B38", "A2-1009_3B38",
        "амперметр", "USB", false, ampChannels
        ));

    // Датчик давления
    QJsonArray pressureUnits;
    pressureUnits << 6 << 7;
    QJsonArray tempUnits;
    tempUnits << 11;

    QJsonArray pressureChannels;
    pressureChannels.append(DeviceGenerator::createChannel(10, "P", "pressure", 7, pressureUnits));
    pressureChannels.append(DeviceGenerator::createChannel(11, "T", "temperature", 11, tempUnits));

    m_devices.append(DeviceGenerator::createSensor(
        "38:3B:26:A2:1E:85", "pressure", "8585", "PM-020_8585",
        "датчик давления", "BLE", false, pressureChannels
        ));

    root["data"] = m_devices;
    return root;
}

QList<QJsonObject> DemoDeviceService::generateData()
{
    QList<QJsonObject> sensorsDataArray;
    // Проходим по всем устройствам в массиве _devices
    for (const QJsonValue& deviceValue : m_devices) {

        QJsonObject device = deviceValue.toObject();

        // Генерируем данные только для подключенных датчиков
        bool connected = device["connected"].toBool();
        if (!connected ) continue;

        QString address = device["address"].toString();
        QJsonArray channels = device["channels"].toArray();

        // Генерируем данные для этого датчика
        QJsonObject sensorData = DeviceGenerator::generateSensorData(address, channels);

        sensorsDataArray.append(sensorData);
    }

    return sensorsDataArray;

}

void DemoDeviceService::thread_proc()
{
    QThread::msleep(1000);

    while (1)
    {
        if (!m_dataEnabled) break;
        QList<QJsonObject> dat_list = generateData();

        for (QJsonObject dat: dat_list) {
            QJsonDocument doc(dat);
            qDebug() << "Generating data " << doc.toJson(QJsonDocument::Compact) << '\n';

            DataFlow dFlow;
            dFlow.deviceMac = dat["address"].toString();
            QJsonObject ch_values =  dat["data"].toObject();
            dFlow.timestamp = ch_values["0"].toInteger();

            for (auto ch_ind: ch_values.keys()) {
                int i = ch_ind.toInt();
                dFlow.values[i] = ch_values[ch_ind].toDouble();
                //          qDebug() << "[STREAM]: " << "device mac" << dFlow.deviceMac << "ch id" << chId << ", value" << value.dataValue();
            }

            emit dataRecieve(dFlow);
        }

        QThread::msleep(50);
    }
}
