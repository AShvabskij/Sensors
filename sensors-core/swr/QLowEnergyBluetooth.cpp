#include "QLowEnergyBluetooth.h"
using namespace swr;

//int QLowEnergyBluetooth::sCarCount = 0;
QLowEnergyBluetooth::QLowEnergyBluetooth(const QBluetoothDeviceInfo& info, QObject* parent) : \
    QBluetoothBase(info, parent)
{
    dataLoggerInfo.deviceType = BLECONNECT;
    bleController = QLowEnergyController::createCentral(info);
    qRegisterMetaType<QLowEnergyConnectionParameters>("QLowEnergyConnectionParameters");
    qRegisterMetaType<QLowEnergyCharacteristic>("QLowEnergyCharacteristic");
    qRegisterMetaType<QLowEnergyDescriptor>("QLowEnergyDescriptor");
    qRegisterMetaType<QLowEnergyController::Error>("QLowEnergyController::Error");

    connect(bleController, SIGNAL(serviceDiscovered(QBluetoothUuid)),
            this, SLOT(onServiceDiscovered(QBluetoothUuid)), Qt::QueuedConnection);
    connect(bleController, SIGNAL(discoveryFinished()),
            this, SLOT(onServiceScanDone()), Qt::QueuedConnection);
    connect(bleController, SIGNAL(errorOccurred(QLowEnergyController::Error)),
            this, SLOT(onControllerError(QLowEnergyController::Error)), Qt::QueuedConnection);
    connect(bleController, SIGNAL(connected()),
            this, SLOT(onDeviceConnected()), Qt::QueuedConnection);
    connect(bleController, SIGNAL(disconnected()),
            this, SLOT(onDeviceDisconnected()), Qt::QueuedConnection);
    connect(bleController, SIGNAL(connectionUpdated(const QLowEnergyConnectionParameters&)),
            this, SLOT(connectionUpdated(const QLowEnergyConnectionParameters&)), Qt::QueuedConnection);
}

QLowEnergyBluetooth::~QLowEnergyBluetooth()
{
}

void QLowEnergyBluetooth::connectDevice()
{
    qDebug() << "QLowEnergyBluetooth::connectDevice 1";
    if(dataLoggerInfo.connectState != dataLoggerStateDisconnected) {
        return;
    }
    dataLoggerInfo.connectState = dataLoggerStateConnecting;
    qDebug() << "QLowEnergyBluetooth::connectDevice 2";
    bleController->connectToDevice();
    qDebug() << "QLowEnergyBluetooth::connectDevice 3";
}

void QLowEnergyBluetooth::disConnectDevice()
{
    if(dataLoggerInfo.connectState != dataLoggerStateConnected) {
        return;
    }
    if(!bleController || bleController->state() == QLowEnergyController::UnconnectedState || \
       bleController->state() == QLowEnergyController::ClosingState) {
        return;
    }
    if (m_notificationDesc.isValid() && bleServer) {
        bleServer->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0000"));
    }
    bleController->disconnectFromDevice();
//        if(bleServer)
//        {
//            delete bleServer;
//            bleServer = nullptr;
//        }
}

void QLowEnergyBluetooth::Write(QByteArray* buf)
{
//    if(bleServer && bleWriteCharacteristic.isValid()){
//        bleServer->writeCharacteristic(bleWriteCharacteristic,*buf);
//        qDebug() << buf->toHex().data();
//    }
    Write(reinterpret_cast<unsigned char*>(buf->data()), buf->length());
}

void QLowEnergyBluetooth::Write(unsigned char* buf, int length)
{
    if(bleServer && bleWriteCharacteristic.isValid()) {
        int n = length / 20;
        int l = length % 20;
        for (int i = 0; i < n; i++) {
            QByteArray sbuf(reinterpret_cast<char*>(&buf[i * 20]), 20/*length*/);
            bleServer->writeCharacteristic(bleWriteCharacteristic, sbuf);
            qDebug() << sbuf.toHex().data();
        }
        QByteArray sbuf(reinterpret_cast<char*>(&buf[n * 20]), l/*length*/);
        bleServer->writeCharacteristic(bleWriteCharacteristic, sbuf);
        qDebug() << "QLowEnergyBluetooth-Write:" << sbuf.toHex().data();
    }
}


void QLowEnergyBluetooth::searchCharacteristic()
{
    if(bleServer) {
//        bool ok = false;
        foreach (QLowEnergyCharacteristic c, bleServer->characteristics()) {
            if(c.isValid() && ( c.uuid() == QBluetoothUuid(QString(UUIDSTR_ISSC_TRANS_Write)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_BLE_TRANS_Write)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_NEWBLE_TRANS_Write)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_HM11_TRANS_Write))
                                )) {
                bleWriteCharacteristic = c;
                qDebug() << "bleWriteCharacteristic:" << bleWriteCharacteristic.isValid();
            }
            if(c.isValid() && ( c.uuid() == QBluetoothUuid(QString(UUIDSTR_ISSC_TRANS_Read)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_BLE_TRANS_Read)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_NEWBLE_TRANS_Read)) ||
                                c.uuid() == QBluetoothUuid(QString(UUIDSTR_HM11_TRANS_Read))
                                )) {
                m_notificationDesc = c.descriptor(\
                        QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
                qDebug() << "m_notificationDesc:" << m_notificationDesc.isValid();
                if (m_notificationDesc.isValid()) {
                    bleServer->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));//使能通知
//                    ok = true;
                    dataLoggerInfo.connectState = dataLoggerStateConnected;
//                    emit updateDeviceState(dataLoggerInfo);
                }
            }
        }
    }
}

void QLowEnergyBluetooth::onDeviceConnected()
{
    qDebug() << "QLowEnergyBluetooth::onDeviceConnected 1";
    m_servicesUuid.clear();
    bleController->discoverServices();
    qDebug() << "QLowEnergyBluetooth::onDeviceConnected 2";
}

void QLowEnergyBluetooth::onDeviceDisconnected()
{
    if(bleServer) {
        delete bleServer;
        bleServer = nullptr;
    }
    dataLoggerInfo.connectState = dataLoggerStateDisconnected;
    emit updateDeviceState(dataLoggerInfo);
}

void QLowEnergyBluetooth::onServiceDiscovered(const QBluetoothUuid&)
{
}

void QLowEnergyBluetooth::onServiceScanDone()
{
    m_servicesUuid = bleController->services();
    if(m_servicesUuid.isEmpty()) {
    } else {
        foreach (auto i, m_servicesUuid) {
            if(i == QBluetoothUuid(QString(SERVICE_UUID)) ||
                i == QBluetoothUuid(QString(BLE_SERVICE_UUID)) ||
                i == QBluetoothUuid(QString(NEWBLE_SERVICE_UUID)) ||
                i == QBluetoothUuid(QString(HM11_SERVICE_UUID))
                ) {
                if(bleServer) {
                    delete bleServer;
                    bleServer = nullptr;
                }
                bleServer = bleController->createServiceObject(i, this);
                if (!bleServer) {
//                    emit statusInfoChanged("Service not found.", false);
                    return;
                }
                connect(bleServer, SIGNAL(stateChanged(QLowEnergyService::ServiceState)),
                        this, SLOT(onServiceStateChanged(QLowEnergyService::ServiceState)), Qt::QueuedConnection);
                qDebug() << "onServiceScanDone - connect characteristicChanged";
                connect(bleServer, SIGNAL(characteristicChanged(QLowEnergyCharacteristic, QByteArray)),
                        this, SLOT(onCharacteristicChanged(QLowEnergyCharacteristic, QByteArray)), Qt::QueuedConnection);
                connect(bleServer, SIGNAL(errorOccurred(QLowEnergyService::ServiceError)),
                        this, SLOT(serviceError(QLowEnergyService::ServiceError)), Qt::QueuedConnection);
                connect(bleServer, SIGNAL(descriptorWritten(const QLowEnergyDescriptor&, const QByteArray&)),
                        this, SLOT(descriptorWritten(const QLowEnergyDescriptor&, const QByteArray&)), Qt::QueuedConnection);
                if(bleServer->state() == QLowEnergyService::DiscoveryRequired) {
//                    emit statusInfoChanged("Connecting to service...", true);
                    bleServer->discoverDetails();
                } else {
                    searchCharacteristic();
                }
            }
        }
    }
}

void QLowEnergyBluetooth::onControllerError(QLowEnergyController::Error err)
{
    if(err != QLowEnergyController::NoError /*|| err == QLowEnergyController::RemoteHostClosedError*/) {
        dataLoggerInfo.connectState = dataLoggerStateDisconnected;
        emit updateDeviceState(dataLoggerInfo);
    }
    qDebug() << "BLE-onControllerError:" << err;
}

void QLowEnergyBluetooth::connectionUpdated(const QLowEnergyConnectionParameters& newParameters)
{
    qDebug() << "latency:" << newParameters.latency() << "-minimumInterval:" << newParameters.minimumInterval()\
             << "-maximumInterval:" << newParameters.maximumInterval() << "-supervisionTimeout:" << newParameters.supervisionTimeout();
}

void QLowEnergyBluetooth::onServiceStateChanged(QLowEnergyService::ServiceState s)
{
    qDebug() << "serviceStateChanged, state: " << s;
    qDebug() << bleServer->error();
    if (s == QLowEnergyService::ServiceDiscovered) {
        searchCharacteristic();
    }
}

void QLowEnergyBluetooth::onCharacteristicChanged(const QLowEnergyCharacteristic& c, const QByteArray& value)
{
    qDebug() << "onCharacteristicChanged:" << c.uuid();
    if(c.uuid() == QBluetoothUuid(QString(UUIDSTR_ISSC_TRANS_Read)) ||
        c.uuid() == QBluetoothUuid(QString(UUIDSTR_BLE_TRANS_Read)) ||
        c.uuid() == QBluetoothUuid(QString(UUIDSTR_NEWBLE_TRANS_Read)) ||
        c.uuid() == QBluetoothUuid(QString(UUIDSTR_HM11_TRANS_Read))) {
        qDebug() << value.toHex().data();
        R_DATA_FLOW revice;
        revice.dataLoggerInfo = dataLoggerInfo;
        revice.dataPacket = vector<uchar>(value.begin(), value.end());
        emit dataRevice(revice);
    }
}

void QLowEnergyBluetooth::serviceError(QLowEnergyService::ServiceError e)
{
    //
    qDebug() << e;
    qDebug() << bleServer->error();
}

void QLowEnergyBluetooth::descriptorWritten(const QLowEnergyDescriptor& descriptor, const QByteArray& newValue)
{
    Q_UNUSED(descriptor)
    Q_UNUSED(newValue)
//    descriptor.value()

    QTimer::singleShot(100, this, [ = ]() {
        qDebug() << "descriptorWritten successfully :" << QString::fromStdString(dataLoggerInfo.deviceMac) ;
        emit updateDeviceState(dataLoggerInfo);
    });
}

