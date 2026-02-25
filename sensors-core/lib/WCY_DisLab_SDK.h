#ifndef WCY_DISLAB_SDK_H
#define WCY_DISLAB_SDK_H

// 跨平台导出宏
#ifdef _WIN32
#define DISLAB_SDK __declspec(dllexport)
#else
#define DISLAB_SDK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	/// <summary>
	/// 设备扫描对象 1.BLE 2.HID
	/// </summary>
	typedef struct DeviceScan
	{
		char deviceName[128];//设备名称
		char macAddress[18];//mac地址
		int deviceType;//设备类型1.BLE 2.HID
	};
	typedef struct ComboFetcher
	{
		int rangeId;
		char rangeName[128];
		double k;
		double upperLimit;
		double lowerLimit;
	};

	typedef struct DataCallback
	{
		ComboFetcher comboFetcher;
		double data;
	};

	typedef struct SwitchRange
	{
		int rangeId;
		double upperLimit;
		double lowerLimit;
	};

	typedef struct Frequency
	{
		int frequency;
	};

	//初始化
	DISLAB_SDK bool Initialize();

	//设置扫描回调
	typedef void(*DEVICE_SCAN_CALLBACK)(const DeviceScan* deviceScan);
	DISLAB_SDK void SetDeviceScan(DEVICE_SCAN_CALLBACK scanCalllback);

	//蓝牙扫描
	DISLAB_SDK void BLEScan(int timeout_seconds);

	//HID扫描
	DISLAB_SDK int HIDScan(DeviceScan** devices,int* outCount);

	//设置数据回调
	typedef void(*DATA_CALLBACK)(const DataCallback* dataCallback);
	DISLAB_SDK void SetDataCallback(DATA_CALLBACK dataCallback);

	//设置设备移除回调
	typedef void(*DEVICE_REMOVED_CALLBACK)(const DeviceScan& deviceScan);
	DISLAB_SDK void SetDeviceRemovedCallback(DEVICE_REMOVED_CALLBACK deviceRemovedCallback);

	//连接
	DISLAB_SDK bool Connect(const DeviceScan* deviceScan);

	//断开连接
	DISLAB_SDK void DisConnect(const DeviceScan* deviceScan);

	//开始
	DISLAB_SDK void Start(const DeviceScan* deviceScan);

	//停止
	DISLAB_SDK void Stop(const DeviceScan* deviceScan);

	//获取输出值组合
	DISLAB_SDK int GetDataTypes(const DeviceScan* deviceScan, ComboFetcher** comboFetchers, int* outCount);
	DISLAB_SDK void FreeDataTypes(ComboFetcher* array);

	//设置传感器语言
	DISLAB_SDK bool SetSensorLang(const DeviceScan* deviceScan, int langeType);

	//获取可切换量程
	DISLAB_SDK int GetSwitchRanges(const DeviceScan* deviceScan, SwitchRange** ranges, int* outCount);
	DISLAB_SDK void FreeSwitchRanges(SwitchRange* array);

	//切换量程
	DISLAB_SDK bool SetSwitchRange(const DeviceScan* deviceScan, int rangeId);

	//获取采样频率
	DISLAB_SDK int GetFrequency(const DeviceScan* deviceScan);

	//获取设备支持的采样频率
	DISLAB_SDK int GetSupportFrequency(const DeviceScan* deviceScan, Frequency** frequency, int* outCount);
	DISLAB_SDK void FreeSupportFrequency(Frequency* array);

	//设置采样频率
	DISLAB_SDK bool SetFrequency(const DeviceScan* deviceScan, int frequency);

	//调零
	DISLAB_SDK bool ClearZero(const DeviceScan* deviceScan);

	//退出
	DISLAB_SDK void Shutdown(const DeviceScan* deviceScan);

	//释放资源
	DISLAB_SDK void Dispose();


#ifdef __cplusplus
}
#endif

#endif // !WCY_DISLAB_SDK_H