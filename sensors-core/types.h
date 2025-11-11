#ifndef TYPES_H
#define TYPES_H

// #include <stdio.h>
// #include <iostream>
// #include <list>
// #include <map>
// #include <thread>
// #include <array>
// #include <vector>
// #include <string>
// #include <future>
// #include <set>


#include <QString>
#include <QObject>


typedef signed char int8;         /* 8 bit signed */
typedef unsigned char uint8;      /* 8 bit unsigned */
typedef short int16;              /* 16 bit signed */
typedef unsigned short uint16;    /* 16 bit unsigned */
typedef long long int64;
typedef int int32;
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char uchar;

#define _return_t long

#define _return_FAIL	0
#define _return_OK		1
#define _return_Ready	2
#define _return_Busy	3

using namespace std;

enum CONNECT_TYPE{
    UNKNOWN = -1,
    USBCONNECT = 0,
    COMCONNECT,
    BLECONNECT,
    BTHCONNECT
};

constexpr const char* ConnectTypeToString(CONNECT_TYPE type) {
    switch(type) {
    case UNKNOWN:    return "UNKNOWN";
    case USBCONNECT: return "USBCONNECT";
    case COMCONNECT: return "COMCONNECT";
    case BLECONNECT: return "BLECONNECT";
    case BTHCONNECT: return "BTHCONNECT";
    default:         return "INVALID";
    }
}

enum CONNECT_STATE{
    StateDisconnected = 0,
    StateConnecting,
    StateConnected,
    StateDisconnecting
};

struct ChannelInfo {
    int id = 0;
    string symbol = "";
    string name = "";
    int baseUnit = 0;
    int availableUnits[10];

    ChannelInfo() {
        std::fill(std::begin(availableUnits), std::end(availableUnits), 0);
    }
};

typedef struct DeviceInfo {
    static const int MAX_CHANNELS = 3;

    int id = 0; // internal unique number
    string deviceId = ""; // external id
    string deviceName = "";
    string deviceMac = "";
    string deviceType = "";
    string displayType = ""; // name to be displayed

    CONNECT_TYPE connectType = CONNECT_TYPE::UNKNOWN;
    CONNECT_STATE connectState = CONNECT_STATE::StateDisconnected;
    ChannelInfo channels[MAX_CHANNELS];

    DeviceInfo() = default;

    DeviceInfo(const DeviceInfo &dInfo)
        : id(dInfo.id),
        deviceId(dInfo.deviceId),
        deviceName(dInfo.deviceName),
        deviceMac(dInfo.deviceMac),
        deviceType(dInfo.deviceType),
        displayType(dInfo.displayType),
        connectType(dInfo.connectType),
        connectState(dInfo.connectState)
    {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            channels[i] = dInfo.channels[i];
        }
    }

    DeviceInfo& operator=(const DeviceInfo &dInfo) {
        if (this != &dInfo) {
            id = dInfo.id;
            deviceId = dInfo.deviceId;
            deviceName = dInfo.deviceName;
            deviceMac = dInfo.deviceMac;
            deviceType = dInfo.deviceType;
            displayType = dInfo.displayType;
            connectType = dInfo.connectType;
            connectState = dInfo.connectState;

            for (int i = 0; i < MAX_CHANNELS; i++) {
                channels[i] = dInfo.channels[i];
            }
        }
        return *this;
    }

} dInfo;

typedef struct DataFlow
{
    int deviceId;
    string deviceMac;
    int64 timestamp;
    std::map<int/*channelId*/, double> values;
} R_DATA_FLOW;

class IDeviceService : public QObject
{
    Q_OBJECT
public:
    virtual ~IDeviceService() {}
    virtual void startScan() = 0;
    virtual void stopScan() = 0;
    virtual void deviceStartStream(const QString& deviceMac) = 0;
    virtual void deviceDisconnect(const QString& deviceMac) = 0;

    virtual void clearDevice() = 0;

    virtual const QList<DeviceInfo> getCurrentConnectDevice() = 0;

// signals:
    Q_SIGNAL void  updatedeviceStatus(IDeviceService* source, DeviceInfo deviceInfo);
    Q_SIGNAL void dataRecieve(DataFlow data);
    Q_SIGNAL void scanFinished(IDeviceService* source);
};

#endif // TYPES_H
