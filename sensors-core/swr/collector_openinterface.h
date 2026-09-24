#ifndef COLLECTOR_OPENINTERFACE_H
#define COLLECTOR_OPENINTERFACE_H

#include "collector_OpenInterface_global.h"


extern "C"{
/*******************************调用接口**************************/
//Initialize Library
COLLECTOR_OPENINTERFACE_EXPORT void initAPI();
//Release Library
COLLECTOR_OPENINTERFACE_EXPORT void releaseAPI();
//Add collector data
COLLECTOR_OPENINTERFACE_EXPORT void appendDeviceData(COLLECTOR_R_DATA_FLOW_C data);
//Update Collector
COLLECTOR_OPENINTERFACE_EXPORT void updateDevice(COLLECTOR_DataLoggerInfo_C info);
//Start continuous collection
COLLECTOR_OPENINTERFACE_EXPORT void startContinuouslyCollect();
//Stop collecting
COLLECTOR_OPENINTERFACE_EXPORT void stopCollect();
//Start single collection
COLLECTOR_OPENINTERFACE_EXPORT void startSingleCollect();
//Set collection interval
COLLECTOR_OPENINTERFACE_EXPORT void setCollectInterval(COLLECTOR_INTERVAL_C interval);
//Obtain sensor information
COLLECTOR_OPENINTERFACE_EXPORT void getSensorInfo();
//Demarcate Sensor
COLLECTOR_OPENINTERFACE_EXPORT void DemarcateSensor(COLLECTOR_DemarcateInfo_C info);
//Calibration Sensor
COLLECTOR_OPENINTERFACE_EXPORT void CalibrationSensor(COLLECTOR_CalibrationInfo_C info);


//

/*******************************设置回调**************************/
//Register command transmission callback
COLLECTOR_OPENINTERFACE_EXPORT void registerCMDTransferCall(CMDTransferCallback call);
//Register sensor update callback
COLLECTOR_OPENINTERFACE_EXPORT void registerSensorUpdateCall(SensorInfoCallback call);
//Registration data update callback
COLLECTOR_OPENINTERFACE_EXPORT void registerDataUpdateCall(DataLoggerDataCallback call);
//
//COLLECTOR_OPENINTERFACE_EXPORT void registerLogCall(LOGCallback call);



}

#endif // COLLECTOR_OPENINTERFACE_H
