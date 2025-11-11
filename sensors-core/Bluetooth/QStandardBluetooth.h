#ifndef QSTANDARDBLUETOOTH_H
#define QSTANDARDBLUETOOTH_H

#include <QBluetoothLocalDevice>
#include <qbluetoothaddress.h>
#include <qbluetoothdevicediscoveryagent.h>
#include <QBluetoothSocket>
#include <QMetaEnum>
#include <QObject>
#include <QDebug>
#include <QList>
#include <QMutex>
#include <future>
#include <QThread>
#include <QTimer>
#include "QBluetoothBase.h"
#define SENDBUFFERLEN  64

class QStandardBluetooth : public QBluetoothBase
{
    Q_OBJECT

public:
    explicit QStandardBluetooth(const QBluetoothDeviceInfo &info,QObject *parent = nullptr);
    virtual ~QStandardBluetooth();
    virtual void connectDevice();
    virtual void disConnectDevice();
    //写数据
    virtual void Write(QByteArray* buf);
    virtual void Write(unsigned char* buf,int length);
    //开始工作
    /*virtual*/ void startWork();
    //停止工作
    /*virtual*/ void stopWork();

protected:
    virtual void run();
    void read();
public slots:
    void readSocket();
    void bthConnected();
    void bthDisconnected();
    void onSocketErrorOccurred(QBluetoothSocket::SocketError);
private:
    QBluetoothServiceInfo serviceInfo;
    QBluetoothSocket* socket = nullptr;
    QBluetoothDeviceInfo bthinfo;
    QBluetoothAddress addr;
    //连接状态锁
    QMutex connLock;
    //线程锁
    QMutex threadLock;
    //运行状态
    bool runState = false;
    //拼接缓存长度
    int connectState;//连接状态
    void initDevice();

};


#endif // QStandardBluetooth_H
