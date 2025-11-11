#ifndef SWR_DEVICESERVICE_H
#define SWR_DEVICESERVICE_H

#include "types.h"

#include <QObject>
#include <QDebug>
#include "Bluetooth/qbluetoothdevicemanagement.h"
#include "collector_OpenInterface_global.h"

class SWR_DeviceService : public QObject,
                          public IDeviceService
{
     Q_OBJECT

public:
    SWR_DeviceService();

    static SWR_DeviceService* instance() {
        static SWR_DeviceService i;
        return &i;
    }

    //命令传输回调
    static void cmdTransferCallback(char deviceMac[20],char *data,uint64 len);
    static void logCallback(const char *data,uint64 len);
    static void SensorInfoCallback(COLLECTOR_SensorInfo_C *rsInfoMap, uint64 lenth);

    static void DataLoggerDataCallback(COLLECTOR_DataLoggerDataInfo_C *dataInfoMap, uint64 len);

    void upDateSensorInfo(COLLECTOR_SensorInfo_C *rsInfoMap, uint64 lenth);
    void upDateDataInfo(COLLECTOR_DataLoggerDataInfo_C *dataInfoMap, uint64 len);

    void sendData(QString deviceMac,QByteArray datArray);
public slots:
    //追加日志
    void appendLogSlots(QString text);
signals:
    void appendLog(QString text);
public slots:
    void updatedeviceStatus(dInfo dataLoggerInfo);//设备状态更新
    void dataRevice(R_DATA_FLOW data);//数据接收


private:
    QBluetoothDeviceManagement* bleManagement = nullptr;

    int m_ConnectType;

    qint64 times = 0;

};

#endif // SWR_DEVICESERVICE_H
