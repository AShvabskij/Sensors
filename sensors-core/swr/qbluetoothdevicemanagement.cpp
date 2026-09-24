#include "qbluetoothdevicemanagement.h"

#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QDebug>

using namespace swr;

QBluetoothDeviceManagement* QBluetoothDeviceManagement::bluetoothManagementObj = nullptr;

QBluetoothDeviceManagement* QBluetoothDeviceManagement::getBluetoothManagement()
{
    if(!bluetoothManagementObj) {
        bluetoothManagementObj = new QBluetoothDeviceManagement();
    }
    return bluetoothManagementObj;
}

void QBluetoothDeviceManagement::releasetBluetoothManagement()
{
    if(bluetoothManagementObj) {
        delete bluetoothManagementObj;
    }
}

QBluetoothDeviceManagement::QBluetoothDeviceManagement()
{
    qRegisterMetaType<dInfo>("swrdInfo");
    qRegisterMetaType<R_DATA_FLOW>("R_DATA_FLOW");
    InitBluetooth();
    nameFiterList.clear();
    QSettings* configIni = new QSettings("config.ini", QSettings::IniFormat);
    {
        configIni->beginGroup("Bluetooth");
        nameFiterList = configIni->value("NameFilter").toStringList();
        configIni->endGroup();
    }
    qDebug() << "nameFiterList:" << nameFiterList;
}

QBluetoothDeviceManagement::~QBluetoothDeviceManagement()
{
}

void QBluetoothDeviceManagement::startScan()
{
    discoveryAgent->stop();
    foreach (auto i, connectDeviceList) {
        emit updatedeviceStatus(i->dataLoggerInfo);
    }
    discoveryAgent->start();
}

void QBluetoothDeviceManagement::stopScan()
{
    discoveryAgent->stop();
}

void QBluetoothDeviceManagement::deviceConnect(const QString& deviceMac)
{
    stopScan();
    if(connectDeviceList.contains(deviceMac)) {
//        已连接
        connectDeviceList.find(deviceMac).value()->connectDevice();
    } else {
        if(bthDeviceInfoList.contains(deviceMac)) {
            QBluetoothDeviceInfo info = bthDeviceInfoList.find(deviceMac).value();
            QBluetoothBase* bthDevice = nullptr;
            if(info.coreConfigurations() == QBluetoothDeviceInfo::BaseRateCoreConfiguration) {
                //标准蓝牙 2.0
                bthDevice = new QStandardBluetooth(bthDeviceInfoList.find(deviceMac).value());
            } else if (info.coreConfigurations() == QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration || info.coreConfigurations() == QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
                //双模
                bthDevice = new QLowEnergyBluetooth(bthDeviceInfoList.find(deviceMac).value());
            } else {
                //未知
                return;
            }
            connect(bthDevice, SIGNAL(updateDeviceState(swrdInfo)), this, SLOT(updatedeviceStatus_SLOT(swrdInfo)), Qt::QueuedConnection);
            connect(bthDevice, SIGNAL(dataRevice(R_DATA_FLOW)), this, SIGNAL(dataRevice(R_DATA_FLOW)), Qt::QueuedConnection);
            connectDeviceList.insert(deviceMac, bthDevice);
            bthDevice->connectDevice();
//          bthDeviceInfoList.remove(deviceMac);
        }
    }
}

void QBluetoothDeviceManagement::deviceDisConnect(const QString& deviceMac)
{
    if(connectDeviceList.contains(deviceMac)) {
        QBluetoothBase* bthDevice = connectDeviceList.find(deviceMac).value();
        bthDevice->disConnectDevice();
        bthDevice->deleteLater();
        connectDeviceList.remove(deviceMac);
    }
}

void QBluetoothDeviceManagement::sendData(string deviceMac, vector<unsigned char> data)
{
    if(connectDeviceList.contains(QString::fromStdString(deviceMac))) {
        connectDeviceList.find(QString::fromStdString(deviceMac)).value()->Write(data.data(), static_cast<int>(data.size()));
    }
}

void QBluetoothDeviceManagement::clearDevice()
{
    stopScan();
    foreach (auto i, connectDeviceList) {
        i->disConnectDevice();
        i->deleteLater();
    }
    bthDeviceInfoList.clear();
}


const QVector<dInfo> QBluetoothDeviceManagement::getCurrentConnectDevice()
{
    QVector<dInfo> dInfoVector;
    foreach (auto i, connectDeviceList) {
        dInfoVector.append(i->dataLoggerInfo);
    }
    return  dInfoVector;
}

void QBluetoothDeviceManagement::InitBluetooth()
{
    #ifndef Q_OS_WIN
    #error "This solution is for Windows only"
    #endif

    auto devList = QBluetoothLocalDevice::allDevices();

    localDevice = new QBluetoothLocalDevice(this);
    if(!localDevice->isValid()) {
        return;
    }

    localDevice->powerOn();
    localDevice->setHostMode(QBluetoothLocalDevice::HostDiscoverable);

    discoveryAgent = new QBluetoothDeviceDiscoveryAgent();
    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)), \
            this, SLOT(discoverDevice(QBluetoothDeviceInfo))/*,Qt::QueuedConnection*/);
    connect(discoveryAgent, SIGNAL(finished()), this, SIGNAL(scanFinished())/*,Qt::QueuedConnection*/);
    connect(localDevice, SIGNAL(pairingFinished(QBluetoothAddress, QBluetoothLocalDevice::Pairing))
            , this, SLOT(pairingDone(QBluetoothAddress, QBluetoothLocalDevice::Pairing))/*,Qt::QueuedConnection*/);}

void QBluetoothDeviceManagement::discoverDevice(const QBluetoothDeviceInfo& info)
{
    if(!info.isValid()) {
        return;
    }
    qDebug() << info.name() << ":" << info.coreConfigurations();
    QString name = info.name().left(2);
    name = name.toUpper();
    bool ok = false;
    if(!nameFiterList.isEmpty()) {
        foreach (auto i, nameFiterList) {
            if(name.contains(i)) {
                ok = true;
                break;
            }
        }
    } else {
        ok = true;
    }
    if(!ok) {
        return;
    }
    if(!info.address().isNull()) {
        bthDeviceInfoList.insert(info.address().toString(), info);
    } else {
        bthDeviceInfoList.insert(info.deviceUuid().toString(), info);
    }
    //发现设备
    dInfo dataLoggerInfo;
    if(!info.address().isNull()) {
        dataLoggerInfo.deviceMac = info.address().toString().toStdString();
    } else {
        dataLoggerInfo.deviceMac = info.deviceUuid().toString().toStdString();
    }
    dataLoggerInfo.deviceName = info.name().toStdString();
    qDebug() << QString::fromStdString(dataLoggerInfo.deviceName) << ":" << QString::fromStdString(dataLoggerInfo.deviceMac);
    if(info.coreConfigurations() == QBluetoothDeviceInfo::BaseRateCoreConfiguration) {
        //标准蓝牙 2.0
        dataLoggerInfo.deviceType = BTHCONNECT;
    } else if (info.coreConfigurations() == QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration) {
        //双模
        dataLoggerInfo.deviceType = BLECONNECT;
    } else if(info.coreConfigurations() == QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        //低功耗蓝牙 4.0
        dataLoggerInfo.deviceType = BLECONNECT;
    } else {
        //未知
        qDebug() << "未知蓝牙类型：" << info.name();
        return;
    }
    dataLoggerInfo.connectState = dataLoggerStateDisconnected;
    emit updatedeviceStatus(dataLoggerInfo);
}

void QBluetoothDeviceManagement::discoverDeviceError(QBluetoothDeviceDiscoveryAgent::Error e)
{
    qDebug() << "QBluetoothDeviceDiscoveryAgent::Error:" << e;
}

void QBluetoothDeviceManagement::discoverDeviceFinshed()
{
    qDebug() << "QBluetoothDeviceManagement::discoverDeviceFinshed";
    QList<QBluetoothDeviceInfo> list = discoveryAgent->discoveredDevices();
    foreach (auto i, list) {
        qDebug() << i.name() << i.coreConfigurations();
    }
}

void QBluetoothDeviceManagement::onSocketErrorOccurred(QBluetoothSocket::SocketError)
{
//    错误
}

void QBluetoothDeviceManagement::pairingDone(const QBluetoothAddress&, QBluetoothLocalDevice::Pairing)
{
}

void QBluetoothDeviceManagement::pairingDisplayConfirmation(const QBluetoothAddress&, QString )
{
}

void QBluetoothDeviceManagement::pairingDisplayPinCode(const QBluetoothAddress&, QString)
{
}

void QBluetoothDeviceManagement::updatedeviceStatus_SLOT(dInfo dataLoggerInfo)
{
    // if (dataLoggerInfo.connectState == dataLoggerStateDisconnected) {
    //     if(connectDeviceList.contains(QString::fromStdString(dataLoggerInfo.deviceMac))) {
    //         QBluetoothBase* bthDevice = connectDeviceList.find(QString::fromStdString(dataLoggerInfo.deviceMac)).value();
    //         delete bthDevice;
    //         bthDevice = nullptr;
    //         connectDeviceList.remove(QString::fromStdString(dataLoggerInfo.deviceMac));
    //     }
    // }
    emit updatedeviceStatus(dataLoggerInfo);
}
