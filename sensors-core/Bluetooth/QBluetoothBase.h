#ifndef QBLUETOOTHBASE_H
#define QBLUETOOTHBASE_H
#include <QObject>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyConnectionParameters>
#include <future>
#include <QTimer>
#include <QThread>
#include "dataloggerstructinfo.h"

#define SERVICE_UUID                           "49535343-FE7D-4AE5-8FA9-9FAFD205E455"
#define UUIDSTR_ISSC_TRANS_Write               "49535343-8841-43F4-A8D4-ECBE34729BB3"
#define UUIDSTR_ISSC_TRANS_Read                "49535343-1E4D-4BD9-BA61-23C647249616"

#define BLE_SERVICE_UUID                       "00001523-1212-efde-1523-785feabcd123"
#define UUIDSTR_BLE_TRANS_Write                "00001525-1212-efde-1523-785feabcd123"
#define UUIDSTR_BLE_TRANS_Read                 "00001524-1212-efde-1523-785feabcd123"

#define NEWBLE_SERVICE_UUID                     "00001523-1212-efde-1523-785fea535752"
#define UUIDSTR_NEWBLE_TRANS_Write             "00001525-1212-efde-1523-785fea535752"
#define UUIDSTR_NEWBLE_TRANS_Read              "00001524-1212-efde-1523-785fea535752"


#define HM11_SERVICE_UUID                       "0000ffe0-0000-1000-8000-00805f9b34fb"
#define UUIDSTR_HM11_TRANS_Write                "0000ffe1-0000-1000-8000-00805f9b34fb"
#define UUIDSTR_HM11_TRANS_Read                 "0000ffe1-0000-1000-8000-00805f9b34fb"

#define HMSERVICE_UUID                            0xCBBB
#define HMCHAR_UUID                               0xCBB1
#define HM_NOTIFY_UUID                            0xCBB1

#define RECVBUFFERLEN  64

class QBluetoothBase: public /*QThread*/QObject
{
    Q_OBJECT
public:
    QBluetoothBase(const QBluetoothDeviceInfo &info, QObject *parent);
    virtual ~QBluetoothBase();
    QString name(){return bluetoothInfo.name();}
    quint64 addr_quint64();
    QString addr_String();

    virtual void connectDevice() = 0;
    virtual void disConnectDevice() = 0;
    virtual void Write(QByteArray* buf) = 0;
    virtual void Write(unsigned char* buf,int length) = 0;

    //获取设备信息
    dInfo getDeviceInfo();

public:
    QBluetoothDeviceInfo bluetoothInfo;
    //设备信息
    dInfo dataLoggerInfo;

signals:
    void dataRevice(R_DATA_FLOW data);
    void updateDeviceState(dInfo dataLoggerInfo);
};

#endif // QBLUETOOTHBASE_H
