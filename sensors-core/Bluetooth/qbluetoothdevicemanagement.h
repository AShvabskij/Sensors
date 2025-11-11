#ifndef QBLUETOOTHDEVICEMANAGEMENT_H
#define QBLUETOOTHDEVICEMANAGEMENT_H
//#include <qbluetoothcommunication.h>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QThread>
#include <QMutex>
//#include <QRegExp>
#include <QMap>
#include "dataloggerstructinfo.h"
#include "QLowEnergyBluetooth.h"
#include "QStandardBluetooth.h"
#include <QSettings>

class QBluetoothDeviceManagement : public QObject
{
    Q_OBJECT
public:
    static QBluetoothDeviceManagement* getBluetoothManagement();
    static void releasetBluetoothManagement();
private:
    QBluetoothDeviceManagement();
    ~QBluetoothDeviceManagement();
public:
    void startScan();
    void stopScan();
    void deviceConnect(const QString& deviceMac);
    void deviceDisConnect(const QString& deviceMac);
    //数据发送
    void sendData(string deviceMac, vector<unsigned char> data);
    //清除设备
    void clearDevice();
    //获取已连接设备信息
    const QVector<dInfo> getCurrentConnectDevice();

protected:
    void InitBluetooth();
    static QBluetoothDeviceManagement* bluetoothManagementObj;
signals:
    void updatedeviceStatus(dInfo dataLoggerInfo);//设备状态更新
    void dataRevice(R_DATA_FLOW data);//数据接收
    void scanFinished();
public slots:
    void discoverDevice(const QBluetoothDeviceInfo& info);
    void discoverDeviceError(QBluetoothDeviceDiscoveryAgent::Error e);
    void discoverDeviceFinshed();


    void onSocketErrorOccurred(QBluetoothSocket::SocketError);
    void pairingDone(const QBluetoothAddress&, QBluetoothLocalDevice::Pairing);
    void pairingDisplayConfirmation(const QBluetoothAddress& address, QString pin);
    void pairingDisplayPinCode(const QBluetoothAddress& address, QString pin);
    void updatedeviceStatus_SLOT(dInfo dataLoggerInfo);
//
private:
    QBluetoothDeviceDiscoveryAgent* discoveryAgent;
    QBluetoothLocalDevice* localDevice;
    QMap<QString, QBluetoothDeviceInfo> bthDeviceInfoList; //扫描蓝牙列表
    QMap<QString, QBluetoothBase*> connectDeviceList; //连接设备列表
    //运行状态
    bool isRunning = false;
    //运行锁
    QMutex runMutex;
    //设备列表锁
    QMutex devListMutex;

    QStringList nameFiterList;
};

#endif // QBLUETOOTHDEVICEMANAGEMENT_H
