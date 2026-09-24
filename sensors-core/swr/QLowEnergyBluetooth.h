//低功耗蓝牙 4.
#ifndef QLOWENERGYBLUETOOTH_H
#define QLOWENERGYBLUETOOTH_H
#include "QBluetoothBase.h"

class QLowEnergyBluetooth : public QBluetoothBase
{
    Q_OBJECT
public:
    explicit QLowEnergyBluetooth(const QBluetoothDeviceInfo& info,QObject *parent = nullptr);
    virtual ~QLowEnergyBluetooth();
    virtual void connectDevice();
    virtual void disConnectDevice();
    //写数据
    virtual void Write(QByteArray* buf);
    virtual void Write(unsigned char* buf,int length);
    //开始停止工作
//    virtual void startWork();
//    virtual void stopWork();

private:
    QLowEnergyController* bleController = nullptr;
    QLowEnergyService* bleServer = nullptr;
    QLowEnergyCharacteristic bleWriteCharacteristic;
    QList<QBluetoothUuid> m_servicesUuid;
    QLowEnergyDescriptor m_notificationDesc;//通知描述符
    void searchCharacteristic();//查询特征
public slots:
    //QLowEnergyController
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onServiceDiscovered(const QBluetoothUuid &);
    void onServiceScanDone();
    void onControllerError(QLowEnergyController::Error);
    //更新连接参数
    void connectionUpdated(const QLowEnergyConnectionParameters &newParameters);

    //QLowEnergyService
    void onServiceStateChanged(QLowEnergyService::ServiceState s);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                              const QByteArray &value);
    void serviceError(QLowEnergyService::ServiceError e);

    void descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue);//描述符写新值成功

};

#endif // QLowEnergyBluetooth_H
