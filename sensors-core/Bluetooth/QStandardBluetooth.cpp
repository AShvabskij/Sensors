#include "QStandardBluetooth.h"

static const QLatin1String serviceUuid("00001101-0000-1000-8000-00805F9B34FB");

QStandardBluetooth::QStandardBluetooth(const QBluetoothDeviceInfo &info, QObject *parent):\
                        QBluetoothBase(info,parent)
{
    addr = info.address();
    dataLoggerInfo.deviceType = BTHCONNECT;
    initDevice();
}

QStandardBluetooth::~QStandardBluetooth()
{
    stopWork();
    if(socket)
    {
        delete socket;
        socket = nullptr;
    }
}

void QStandardBluetooth::initDevice()
{
    socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    connect(socket, &QBluetoothSocket::readyRead, this, &QStandardBluetooth::readSocket,Qt::QueuedConnection);

    connect(socket, &QBluetoothSocket::connected, this, QOverload<>::of(&QStandardBluetooth::bthConnected),Qt::QueuedConnection);

    connect(socket, &QBluetoothSocket::disconnected, this, &QStandardBluetooth::bthDisconnected,Qt::QueuedConnection);
}

void QStandardBluetooth::connectDevice()
{
    socket->connectToService(addr,QBluetoothUuid(serviceUuid) ,QIODevice::ReadWrite);
}

void QStandardBluetooth::disConnectDevice()
{
    //断开连接
    stopWork();
    if(socket->isOpen())
    {
        socket->disconnectFromService();
        socket->close();
    }

}

void QStandardBluetooth::Write(QByteArray *buf)
{
    if(socket->isWritable())
    {
        socket->write(buf->data(),buf->count());
    }
}

void QStandardBluetooth::Write(unsigned char *buf, int length)
{
    if(socket->isWritable())
    {
        socket->write(reinterpret_cast<char*>(buf),length);
    }

}
//! [readSocket]
void QStandardBluetooth::readSocket()
{
    {
        QByteArray readBuffer;
        readBuffer = socket->readAll();

        R_DATA_FLOW revice;
        revice.dataLoggerInfo = dataLoggerInfo;
        revice.dataPacket = vector<uchar>(readBuffer.begin(),readBuffer.end());
        emit dataRevice(revice);

    }
}

void QStandardBluetooth::startWork()
{
}

void QStandardBluetooth::stopWork()
{
}

void QStandardBluetooth::run()
{
    while(1)
    {
        QMutexLocker lock(&threadLock);
        if(!runState)
            break;

        read();
//        usleep(10);
    }
}

void QStandardBluetooth::read()
{
    if (!socket->isOpen())
        return;
}
//! [sendMessage]

void QStandardBluetooth::onSocketErrorOccurred(QBluetoothSocket::SocketError error)
{
//    emit socketErrorOccurred(errorString);
}

//! [connected]
void QStandardBluetooth::bthConnected()
{
    startWork();
    dataLoggerInfo.connectState = dataLoggerStateConnected;
    emit updateDeviceState(dataLoggerInfo);
}
//! [connected]
//!
//! [disconnected]
void QStandardBluetooth::bthDisconnected()
{
    dataLoggerInfo.connectState = dataLoggerStateDisconnected;
}
//! [disconnected]

