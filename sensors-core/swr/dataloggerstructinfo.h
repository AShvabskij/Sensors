/*
 ******************************dataloggerStructInfo******************************
 * brief:数据处理结构信息，包括各种结构体与枚举类型
 * Create time:2020/05/11
 * founder:张子龙
 **************************************************************************************
*/

#ifndef DATALOGGERSTRUCTINFO_H
#define DATALOGGERSTRUCTINFO_H

#include <stdio.h>
#include <iostream>
#include <list>
#include <map>
#include <thread>
#include <array>
#include <vector>
#include <string>
#include <future>
#include <set>
using namespace std;
typedef signed char int8;         /* 8 bit signed */
typedef unsigned char uint8;      /* 8 bit unsigned */
typedef short int16;              /* 16 bit signed */
typedef unsigned short uint16;    /* 16 bit unsigned */
typedef long long int64;
typedef int int32;
typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned char uchar;

//采集器连接类型
namespace swr {
enum CONNECTTYPE{
    UNKNOWN = -1,
    USBCONNECT = 0, //usb连接
    COMCONNECT,     //串口连接
    BLECONNECT,     //低功耗蓝牙
    BTHCONNECT      //标准蓝牙
};

//采集器连接状态
enum CONNECTSTATE{
    dataLoggerStateDisconnected = 0,//未连接
    dataLoggerStateConnecting,      //连接中
    dataLoggerStateConnected,       //已连接
    dataLoggerStateDisconnecting    //断开中
};

//设备基本信息参数
//设备表，储存设备信息
typedef struct DataLoggerInfo{
    string deviceName;  //设备名称
    string deviceMac;   //设备mac地址
    CONNECTTYPE deviceType;   //设备连接类型
    CONNECTSTATE connectState; //连接状态
    DataLoggerInfo(){
        deviceName = "";  //设备名称
        deviceMac = "";   //设备mac地址
        deviceType = UNKNOWN;   //设备连接类型
        connectState = dataLoggerStateDisconnected; //连接状态
    }

     DataLoggerInfo(const DataLoggerInfo &dInfo){
        this->deviceName = dInfo.deviceName;  //设备名称
        this->deviceMac = dInfo.deviceMac;   //设备mac地址
        this->deviceType = dInfo.deviceType;   //设备连接类型
        this->connectState = dInfo.connectState; //连接状态
    }

    void operator = (const DataLoggerInfo &dInfo){
        this->deviceName = dInfo.deviceName;  //设备名称
        this->deviceMac = dInfo.deviceMac;   //设备mac地址
        this->deviceType = dInfo.deviceType;   //设备连接类型
        this->connectState = dInfo.connectState; //连接状态
    }

}dInfo;

typedef struct receiveDataFlow
{
    DataLoggerInfo dataLoggerInfo;  //设备信息
    vector<uchar> dataPacket;          //数据包
}R_DATA_FLOW;

}






#endif // DATALOGGERSTRUCTINFO_H
