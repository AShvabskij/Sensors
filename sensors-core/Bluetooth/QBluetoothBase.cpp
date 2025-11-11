#include "QBluetoothBase.h"

QBluetoothBase::QBluetoothBase(const QBluetoothDeviceInfo &info, QObject *parent) :\
                               /*QThread*/QObject(parent),bluetoothInfo(info)
{
    dataLoggerInfo.deviceName = info.name().toStdString();
    if(!info.address().isNull())
        dataLoggerInfo.deviceMac = info.address().toString().toStdString();
    else
        dataLoggerInfo.deviceMac = info.deviceUuid().toString().toStdString();

    qDebug() << "QBluetoothBase:" << QString::fromStdString(dataLoggerInfo.deviceMac);

    dataLoggerInfo.connectState = dataLoggerStateDisconnected;

}

QBluetoothBase::~QBluetoothBase()
{

}

quint64 QBluetoothBase::addr_quint64()
{
    return bluetoothInfo.address().toString().toULongLong();
}

QString QBluetoothBase::addr_String()
{
    return bluetoothInfo.address().toString();
}

dInfo QBluetoothBase::getDeviceInfo()
{
    return dataLoggerInfo;
}
