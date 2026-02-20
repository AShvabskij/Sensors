#ifndef WCY_DISLAB_SDK_H
#define WCY_DISLAB_SDK_H

// ¿çÆœÌšµŒ³öºê
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
	/// Éè±žÉšÃè¶ÔÏó 1.BLE 2.HID
	/// </summary>
	typedef struct DeviceScan
	{
		char deviceName[128];//Éè±žÃû³Æ
		char macAddress[18];//macµØÖ·
		int deviceType;//Éè±žÀàÐÍ1.BLE 2.HID
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


	//³õÊŒ»¯
	DISLAB_SDK bool Initialize();

	//ÉèÖÃÉšÃè»Øµ÷
	typedef void(*DEVICE_SCAN_CALLBACK)(const DeviceScan* deviceScan);
	DISLAB_SDK void SetDeviceScan(DEVICE_SCAN_CALLBACK scanCalllback);

	//À¶ÑÀÉšÃè
	DISLAB_SDK void BLEScan(int timeout_seconds);

	//HIDÉšÃè
	DISLAB_SDK int HIDScan(DeviceScan** devices,int* outCount);

	//ÉèÖÃÊýŸÝ»Øµ÷
	typedef void(*DATA_CALLBACK)(const DataCallback* dataCallback);
	DISLAB_SDK void SetDataCallback(DATA_CALLBACK dataCallback);

	//ÉèÖÃÉè±žÒÆ³ý»Øµ÷
	typedef void(*DEVICE_REMOVED_CALLBACK)(const DeviceScan& deviceScan);
	DISLAB_SDK void SetDeviceRemovedCallback(DEVICE_REMOVED_CALLBACK deviceRemovedCallback);

	//Á¬œÓ
	DISLAB_SDK bool Connect(const DeviceScan* deviceScan);

	//¶Ï¿ªÁ¬œÓ
	DISLAB_SDK void DisConnect(const DeviceScan* deviceScan);

	//¿ªÊŒ
	DISLAB_SDK void Start(const DeviceScan* deviceScan);

	//Í£Ö¹
	DISLAB_SDK void Stop(const DeviceScan* deviceScan);

	//»ñÈ¡Êä³öÖµ×éºÏ
	DISLAB_SDK int GetDataTypes(const DeviceScan* deviceScan, ComboFetcher** comboFetchers, int* outCount);
	DISLAB_SDK void FreeDataTypes(ComboFetcher* array);

	//ÉèÖÃŽ«žÐÆ÷ÓïÑÔ
	DISLAB_SDK bool SetSensorLang(const DeviceScan* deviceScan, int langeType);

	//»ñÈ¡¿ÉÇÐ»»Á¿³Ì
	DISLAB_SDK int GetSwitchRanges(const DeviceScan* deviceScan, SwitchRange** ranges, int* outCount);
	DISLAB_SDK void FreeSwitchRanges(SwitchRange* array);

	//ÇÐ»»Á¿³Ì
	DISLAB_SDK bool SetSwitchRange(const DeviceScan* deviceScan, int rangeId);

	//»ñÈ¡²ÉÑùÆµÂÊ
	DISLAB_SDK int GetFrequency(const DeviceScan* deviceScan);

	//»ñÈ¡Éè±žÖ§³ÖµÄ²ÉÑùÆµÂÊ
	DISLAB_SDK int GetSupportFrequency(const DeviceScan* deviceScan, Frequency** frequency, int* outCount);
	DISLAB_SDK void FreeSupportFrequency(Frequency* array);

	//ÉèÖÃ²ÉÑùÆµÂÊ
	DISLAB_SDK bool SetFrequency(const DeviceScan* deviceScan, int frequency);

	//µ÷Áã
	DISLAB_SDK bool ClearZero(const DeviceScan* deviceScan);

	//ÍË³ö
	DISLAB_SDK void Shutdown(const DeviceScan* deviceScan);

	//ÊÍ·Å×ÊÔŽ
	DISLAB_SDK void Dispose();


#ifdef __cplusplus
}
#endif

#endif // !WCY_DISLAB_SDK_H
