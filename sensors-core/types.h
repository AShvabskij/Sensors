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

enum SysType
{
    SysType_Undefined = 0,
    Default = 0,
    DEMO = 1,
    WCY = 2,
    SWR = 3,
    SysType_Unknown
};

namespace SYS_TYPES {
inline const char* sysTypeToString(SysType type) {
    switch (type) {
    case DEMO: return "DEMO"; // only for demo mode
    case WCY: return "WCY";
    case SysType_Unknown: return "Unknown";
    default: return "";
    }

    return "";
}

inline SysType sysTypeFromString(const char* type) {
    if (type == nullptr) {
        return SysType_Undefined;
    }

    if (strcmp(type, "DEMO") == 0) return DEMO;
    if (strcmp(type, "WCY") == 0) return WCY;
    return SysType_Unknown;
}
}

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
    StateUnknown = 0,
    StateScanned,
    StateConnecting,
    StateConnected,
    StateDisconnected,
    StateStarted,
    StateRemoved
};

static const int MAX_UNITS = 5;
struct ChannelInfo {

    int id = 0;
    QString symbol = "";
    QString name = "";
    int baseUnit = 0;
    int availableUnits[MAX_UNITS];
    int32 decimal;            //Sensor data display decimal places
    double minValue;          //Minimum value of sensor
    double maxValue;          //Maximum sensor value

    ChannelInfo() {
        std::fill(std::begin(availableUnits), std::end(availableUnits), 0);
    }
};

typedef struct DeviceInfo {
    static const int MAX_CHANNELS = 10;

    int id = 0; // internal unique number
    QString deviceId = ""; // external id
    QString deviceName = "";
    QString deviceMac = "";
    QString deviceType = "";
    QString displayType = ""; // name to be displayed

    CONNECT_TYPE connectType = CONNECT_TYPE::UNKNOWN;
    CONNECT_STATE connectState = CONNECT_STATE::StateUnknown;
    int frequency = 0; // current frequency to receive data
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
        connectState(dInfo.connectState),
        frequency(dInfo.frequency)
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
            frequency = dInfo.frequency;

            for (int i = 0; i < MAX_CHANNELS; i++) {
                channels[i] = dInfo.channels[i];
            }
        }
        return *this;
    }

} dInfo;

struct DataFlow
{
    QString deviceMac;
    int64 timestamp;
    std::map<int/*channelId*/, double> values;
};

class IDeviceService : public QObject
{
    Q_OBJECT
public:
    virtual ~IDeviceService() {}
    virtual void init() = 0;

    virtual void startScan() = 0;
    virtual void stopScan() = 0;
    virtual void deviceStartStream(const QString& deviceMac) = 0;
    virtual void deviceStopStream(const QString& deviceMac) = 0;

    virtual void deviceConnect(const QString& deviceMac) = 0;
    virtual void deviceDisconnect(const QString& deviceMac) = 0;

    virtual void stopAll() = 0;
    virtual void exit() = 0;

    virtual const QList<DeviceInfo> getConnectedDeviceList() = 0;

// signals:
    Q_SIGNAL void  updatedeviceStatus(IDeviceService* source, const DeviceInfo& deviceInfo);
    Q_SIGNAL void dataRecieve(DataFlow data);
    Q_SIGNAL void scanFinished(IDeviceService* source);

};

#endif // TYPES_H
