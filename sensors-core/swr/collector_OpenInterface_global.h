#ifndef COLLECTOR_OPENINTERFACE_GLOBAL_H
#define COLLECTOR_OPENINTERFACE_GLOBAL_H
;
#pragma pack(push,1)
#if defined(_MSC_VER) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define Q_DECL_EXPORT __declspec(dllexport)
#  define Q_DECL_IMPORT __declspec(dllimport)
#else
#  define Q_DECL_EXPORT     __attribute__((visibility("default")))
#  define Q_DECL_IMPORT     __attribute__((visibility("default")))
#endif

#if defined(COLLECTOR_OPENINTERFACE_LIBRARY)
#  define COLLECTOR_OPENINTERFACE_EXPORT Q_DECL_EXPORT
#else
#  define COLLECTOR_OPENINTERFACE_EXPORT Q_DECL_IMPORT
#endif

#include <cstdio>
#include <stdbool.h>
#include <stdio.h>
typedef signed char int8;         /* 8 bit signed */
typedef unsigned char uint8;      /* 8 bit unsigned */
typedef short int16;              /* 16 bit signed */
typedef unsigned short uint16;    /* 16 bit unsigned */
typedef long long int64;
typedef int int32;
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char uchar;

//Collection interval
typedef enum{
    COLLECTOR_5MS = 2,
    COLLECTOR_10MS,
    COLLECTOR_20MS,
    COLLECTOR_50MS,
    COLLECTOR_100MS,
    COLLECTOR_200MS,
    COLLECTOR_500MS,
    COLLECTOR_1S,
    COLLECTOR_2S,
    COLLECTOR_5S,
    COLLECTOR_10S,
    COLLECTOR_20S,
    COLLECTOR_1MIN,
    COLLECTOR_2MIN,
    COLLECTOR_5MIN,
    COLLECTOR_10MIN,
    COLLECTOR_20MIN,
    COLLECTOR_1HOUR,
    COLLECTOR_1MS,
}COLLECTOR_INTERVAL_C;

//Collector connection type
typedef enum{
    COLLECTOR_USBCONNECT = 0,//USB
    COLLECTOR_BLECONNECT,    //bluetooth
}COLLECTOR_CONNECTTYPE_C;

//Collector connection status
typedef enum{
    COLLECTOR_dataLoggerStateDisconnected = 0,    //Disconnect the connection
    COLLECTOR_dataLoggerStateConnected,           //Connected already
}COLLECTOR_CONNECTSTATE_C;

//DataLogger Info
typedef struct {
    char m_Name[100];             //device name
    char m_MACAdress[20];        //device MAC
    COLLECTOR_CONNECTTYPE_C m_ConnectType;      //Device connection type
    COLLECTOR_CONNECTSTATE_C m_ConnectState;      //Connection status
}COLLECTOR_DataLoggerInfo_C;

//DataLogger Data Info
typedef struct{
    bool m_IsSingleCollect;     // Is it a single data collection
    int32 m_HandleID;           //Sensor Handle ID
    double m_Value;             //Sensor Data Value
    bool m_Upside;              //rising edge flg；The use of photoelectric door sensors
    int32 m_ADValueArray[5];    //Sensor AD Value List
    int64 m_Index;              //Sensor Data Index
    double m_GDMTime;           //photogate Sensor Time Value；The use of photoelectric door sensors
}COLLECTOR_DataLoggerDataInfo_C;

typedef struct{
    char m_DeviceMac[20];       //device MACAddress
    int32 m_Channel;            //sensor channel
    int32 m_HandleID;           //Sensor Handle ID
    char m_Name[100];           //Sensor Name
    char m_Unit[100];           //Sensor Unit
    double m_MinValue;          //Minimum value of sensor
    double m_MaxValue;          //Maximum sensor value
    int32 m_Decimal;            //Sensor data display decimal places
}COLLECTOR_SensorInfo_C;

typedef struct{
    COLLECTOR_DataLoggerInfo_C m_dataLoggerInfo;  //DataLogger Info
    char* m_dataPacket;                     //Data Packet
    int64 m_dataSize;                       //Packet Length
} COLLECTOR_R_DATA_FLOW_C;

//标定信息
typedef struct{
    int32 m_HandleID;           //Sensor Handle ID
    int32 m_Offset;               //offset AD Value
    int64 m_LowAD;                //low-end AD Value
    int64 m_HighAD;               //high-end AD Value
    double m_LowValue;            //low-end Value
    double m_HighValue;           //high-end Value
}COLLECTOR_DemarcateInfo_C;

//校准信息
typedef struct {
    int32 m_HandleID;       //Sensor Handle ID
    int64 m_ADValueList[5];   //AD Value List
    double m_CurrentValue;    //current Value
    double m_CalibrateValue;  //calibrate Value
}COLLECTOR_CalibrationInfo_C;


/**********************************
* @brief    Sensor status callback
* @param    rsInfoMap:Sensor Info List；lenth：Sensor Info list length；
* @return   void
************************************/
typedef void(*SensorInfoCallback)(COLLECTOR_SensorInfo_C *rsInfoList,uint64 lenth);
/**********************************
* @brief    Data callback
* @param    dataInfoMap:Data List；len：Data list length；
* @return   void
************************************/
typedef void(*DataLoggerDataCallback)(COLLECTOR_DataLoggerDataInfo_C *dataInfoList,uint64 lenth);
/**********************************
* @brief    CMD Transfer Callback
* @param    deviceMac:device Macaddress ；data:Data to be sent;lenth:Data length；
* @return   void
************************************/
typedef void(*CMDTransferCallback)(char deviceMac[20],char *data,uint64 lenth);
//
//typedef void(*LOGCallback)(const char *data,uint64 len);

#pragma pack (pop)


#endif // COLLECTOR_OPENINTERFACE_GLOBAL_H
