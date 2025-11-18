using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Grpc.Core;
using Microsoft.Extensions.Logging;
using WCY.DisLab.SDK;
using WCY.DisLab.SDK.Devices.Enum;
using WCY.DisLab.SDK.Devices.Model;
using WCY.DisLab.SDK.Dictionary.Model;
using WCY.DisLab.SDK.Model;

using GrpcSensorServer;
using System.Threading;

namespace GrpcSensorServer.Services
{
    public class SensorGrpcService : SensorService.SensorServiceBase
    {
        private ISensorService _sensorService = new WCY.DisLab.SDK.SensorService();

        private readonly Dictionary<string, List<IServerStreamWriter<SensorDataMessage>>> _dataSubscribers
            = new Dictionary<string, List<IServerStreamWriter<SensorDataMessage>>>();
        private readonly Dictionary<string, List<IServerStreamWriter<SensorEventMessage>>> _eventSubscribers
            = new Dictionary<string, List<IServerStreamWriter<SensorEventMessage>>>();

        private readonly Dictionary<string, List<int>> _deviceDataMapping = new Dictionary<string, List<int>>();
        private readonly Dictionary<int/*rangeId*/, WCY.DisLab.SDK.Devices.Model.DataTypeModel> _ranges = new Dictionary<int, WCY.DisLab.SDK.Devices.Model.DataTypeModel>();

        public SensorGrpcService()
        {
            bool initResult = _sensorService.Initialize("en");
            Console.WriteLine($"Sensor service initialized: {initResult}");

            _sensorService.SensorDataHandler += OnSensorDataReceived;
            _sensorService.SensorRemovceHandler += OnSensorRemoved;
        }

        // Simple logging methods
        private void LogInfo(string message)
        {
            Console.WriteLine($"[INFO] {DateTime.Now:yyyy-MM-dd HH:mm:ss} - {message}");
        }

        private void LogError(string message, Exception ex = null)
        {
            Console.WriteLine($"[ERROR] {DateTime.Now:yyyy-MM-dd HH:mm:ss} - {message}");
            if (ex != null)
            {
                Console.WriteLine($"       Exception: {ex.Message}");
                if (ex.InnerException != null)
                {
                    Console.WriteLine($"       Inner Exception: {ex.InnerException.Message}");
                }
            }
        }

        private void LogWarning(string message)
        {
            Console.WriteLine($"[WARN] {DateTime.Now:yyyy-MM-dd HH:mm:ss} - {message}");
        }
        public override async Task<BLEScanResponse> BLEScan(BLEScanRequest request, ServerCallContext context)
        {
            try
            {
                bool initResult = _sensorService.Initialize("en");
                Console.WriteLine($"Sensor service initialized: {initResult}");

                LogInfo($"Starting BLE scan with timeout: {request.Timeout}ms");
                var devices = await _sensorService.BLEScan(request.Timeout);

                var response = new BLEScanResponse();
                response.Devices.AddRange(devices.Select(ToGrpcDeviceModel));

                LogInfo($"BLE scan completed. Found {devices.Count} devices");
                return response;
            }
            catch (Exception ex)
            {
                LogError("Error during BLE scan", ex);
                return new BLEScanResponse { Error = ex.Message };
            }
        }

        public override async Task<USBScanResponse> USBScan(USBScanRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo("Starting USB scan");
                var devices = await _sensorService.USBScan();

                var response = new USBScanResponse();
                 response.Devices.AddRange(devices.Select(ToGrpcDeviceModel));

                LogInfo($"USB scan completed. Found {devices.Count} devices");
                foreach (var device in devices)
                {
                    Console.WriteLine(device.DeviceName + ", Mac:" + device.DeviceMac);
                }

                return response;
            }
            catch (Exception ex)
            {
                LogError("Error during USB scan", ex);
                return new USBScanResponse { Error = ex.Message };
            }
        }

        public override async Task<ConnectResponse> Connect(ConnectRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Connecting to device with MAC: {request.Mac}");
                var result = await _sensorService.ConnectAsync(request.Mac);

                LogInfo($"Connection result for {request.Mac}: {result}");
                return new ConnectResponse { Result = (ConnectEnum)result };
            }
            catch (Exception ex)
            {
                LogError($"Error connecting to device {request.Mac}", ex);
                return new ConnectResponse { Result = ConnectEnum.Exception, Error = ex.Message };
            }
        }

        public override Task<DisconnectResponse> Disconnect(DisconnectRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Disconnecting device: {request.DeviceMac}");
                var result = _sensorService.DisConnect(request.DeviceMac);

                // Remove from data mapping
                lock (_deviceDataMapping)
                {
                    _deviceDataMapping.Remove(request.DeviceMac);
                }

                LogInfo($"Disconnect result for {request.DeviceMac}: {result}");
                return Task.FromResult(new DisconnectResponse { Success = result });
            }
            catch (Exception ex)
            {
                LogError($"Error disconnecting device {request.DeviceMac}", ex);
                return Task.FromResult(new DisconnectResponse { Success = false, Error = ex.Message });
            }
        }

        public override async Task<GetSensorRangeResponse> GetSensorRange(GetSensorRangeRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Getting sensor range for device: {request.DeviceMac}");
                var ranges = await _sensorService.GetSensorRange(request.DeviceMac);

                var response = new GetSensorRangeResponse();
                foreach (var range in ranges)
                {
                    _ranges[range.RangeId] =range;
                }
                response.Ranges.AddRange(ranges.Select(ToGrpcDataTypeModel));

                LogInfo($"Found {ranges.Count} ranges for device {request.DeviceMac}");
                return response;
            }
            catch (Exception ex)
            {
                LogError($"Error getting sensor range for device {request.DeviceMac}", ex);
                return new GetSensorRangeResponse { Error = ex.Message };
            }
        }

        public override async Task<StartSendDataResponse> StartSendData(StartSendDataRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Starting data send for device: {request.DeviceMac}");

                List<WCY.DisLab.SDK.Devices.Model.DataTypeModel> dataTypes = new List<WCY.DisLab.SDK.Devices.Model.DataTypeModel>();

                _deviceDataMapping[request.DeviceMac] = new List<int>();
                foreach (int id in request.RangeIdList)
                {
                    dataTypes.Add(_ranges[id]);
                    _deviceDataMapping[request.DeviceMac].Add(id);

                }

                //                var dataTypes = request.DataTypes.Select(FromGrpcDataTypeModel).ToList();
                var result = await _sensorService.StartSendData(request.DeviceMac, dataTypes);
                if (result == false)
                {
                    lock (_deviceDataMapping)
                    {
                        _deviceDataMapping[request.DeviceMac].Clear();
                    }
                }

                LogInfo($"Start data send result for {request.DeviceMac}: {result}");
                return new StartSendDataResponse { Success = result };
            }
            catch (Exception ex)
            {
                LogError($"Error starting data send for device {request.DeviceMac}", ex);
                return new StartSendDataResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task<StopSendDataResponse> StopSendData(StopSendDataRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Stopping data send for device: {request.DeviceMac}");
                var result = await _sensorService.StopSendData(request.DeviceMac);

                LogInfo($"Stop data send result for {request.DeviceMac}: {result}");
                return new StopSendDataResponse { Success = result };
            }
            catch (Exception ex)
            {
                LogError($"Error stopping data send for device {request.DeviceMac}", ex);
                return new StopSendDataResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task<ClearZeroResponse> ClearZero(ClearZeroRequest request, ServerCallContext context)
        {
            try
            {

                List<WCY.DisLab.SDK.Devices.Model.DataTypeModel> dataTypes = new List<WCY.DisLab.SDK.Devices.Model.DataTypeModel>();
                foreach (int id in request.RangeIdList)
                {
                    dataTypes.Add(_ranges[id]);
                }

                var result = await _sensorService.ClearZero(request.DeviceMac, dataTypes);

                return new ClearZeroResponse { Success = result };
            }
            catch (Exception ex)
            {
                return new ClearZeroResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task<GetSwitchRangeResponse> GetSwitchRange(GetSwitchRangeRequest request, ServerCallContext context)
        {
            try
            {
                LogInfo($"Getting switch range for device: {request.DeviceMac}");
                var ranges = await _sensorService.GetSwitchRange(request.DeviceMac);

                var response = new GetSwitchRangeResponse();
                response.Ranges.AddRange(ranges.Select(r => new SwitchRangeModel
                {
                    RangeId = r.RangeId,
                    LowerLimit = r.LowerLimit,
                    UpperLimit = r.UpperLimit
                }));

                LogInfo($"Found {ranges.Count} switch ranges for device {request.DeviceMac}");

                return response;
            }
            catch (Exception ex)
            {
//                _logger.LogError(ex, "Error getting switch range for device {DeviceMac}", request.DeviceMac);
                return new GetSwitchRangeResponse { Error = ex.Message };
            }
        }

        public override async Task<SetRangeResponse> SetRange(SetRangeRequest request, ServerCallContext context)
        {
            try
            {
//              _logger.LogInformation("Setting range for device: {DeviceMac} to {RangeId}", request.DeviceMac, request.RangeId);

                var result = await _sensorService.SetRange(request.DeviceMac, request.RangeId);

//              _logger.LogInformation("Set range result for {DeviceMac}: {Result}", request.DeviceMac, result);
                return new SetRangeResponse { Success = result };
            }
            catch (Exception ex)
            {
//              _logger.LogError(ex, "Error setting range for device {DeviceMac}", request.DeviceMac);
                return new SetRangeResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task<GetCurrentFrequencyResponse> GetCurrentFrequency(GetCurrentFrequencyRequest request, ServerCallContext context)
        {
            try
            {
//                _logger.LogInformation("Getting current frequency for device: {DeviceMac}", request.DeviceMac);
                var frequency = await _sensorService.GteCrrentFrequency(request.DeviceMac);

//                _logger.LogInformation("Current frequency for {DeviceMac}: {Frequency}", request.DeviceMac, frequency);

                return new GetCurrentFrequencyResponse { Frequency = frequency };
            }
            catch (Exception ex)
            {
//                _logger.LogError(ex, "Error getting current frequency for device {DeviceMac}", request.DeviceMac);
                return new GetCurrentFrequencyResponse { Error = ex.Message };
            }
        }

        public override async Task<GetSupportFrequencyResponse> GetSupportFrequency(GetSupportFrequencyRequest request, ServerCallContext context)
        {
            try
            {
//                _logger.LogInformation("Getting supported frequencies for device: {DeviceMac}", request.DeviceMac);
                var frequencies = await _sensorService.GetSupportFrequency(request.DeviceMac);

                var response = new GetSupportFrequencyResponse();
                response.Frequencies.AddRange(frequencies.Select(f => new FrequencyModel { Frequency = f.Frequency }));

//              _logger.LogInformation("Found {FrequencyCount} supported frequencies for device {DeviceMac}", frequencies.Count, request.DeviceMac);

                return response;
            }
            catch (Exception ex)
            {
//              _logger.LogError(ex, "Error getting supported frequencies for device {DeviceMac}", request.DeviceMac);
                return new GetSupportFrequencyResponse { Error = ex.Message };
            }
        }

        public override async Task<SetFrequencyResponse> SetFrequency(SetFrequencyRequest request, ServerCallContext context)
        {
            try
            {
//              _logger.LogInformation("Setting frequency for device: {DeviceMac} to {Frequency}", request.DeviceMac, request.Frequency);

                var result = await _sensorService.SetFrequency(request.DeviceMac, request.Frequency);

//              _logger.LogInformation("Set frequency result for {DeviceMac}: {Result}", request.DeviceMac, result);
                return new SetFrequencyResponse { Success = result };
            }
            catch (Exception ex)
            {
//              _logger.LogError(ex, "Error setting frequency for device {DeviceMac}", request.DeviceMac);
                return new SetFrequencyResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task<DeviceExitResponse> DeviceExit(DeviceExitRequest request, ServerCallContext context)
        {
            try
            {
//              _logger.LogInformation("Exiting device: {DeviceMac}", request.DeviceMac);
                var result = await _sensorService.DeviceExit(request.DeviceMac);

                // Clean up subscriptions
                CleanupDeviceSubscriptions(request.DeviceMac);

//              _logger.LogInformation("Device exit result for {DeviceMac}: {Result}", request.DeviceMac, result);
                return new DeviceExitResponse { Success = result };
            }
            catch (Exception ex)
            {
//              _logger.LogError(ex, "Error exiting device {DeviceMac}", request.DeviceMac);
                return new DeviceExitResponse { Success = false, Error = ex.Message };
            }
        }

        public override async Task SubscribeToSensorData(SubscribeRequest request,
            IServerStreamWriter<SensorDataMessage> responseStream, ServerCallContext context)
        {
            var deviceMac = request.DeviceMac;
            LogInfo($"Starting sensor data subscription for device: {deviceMac}");

            lock (_dataSubscribers)
            {
                if (!_dataSubscribers.ContainsKey(deviceMac))
                    _dataSubscribers[deviceMac] = new List<IServerStreamWriter<SensorDataMessage>>();
                _dataSubscribers[deviceMac].Add(responseStream);
            }

            try
            {
                // Keep alive - send periodic heartbeats
                while (!context.CancellationToken.IsCancellationRequested)
                {
                    await Task.Delay(5000, context.CancellationToken);

                    // Send empty message as heartbeat
                    var heartbeat = new SensorDataMessage();
                    await responseStream.WriteAsync(heartbeat);
                }
            }
            catch (TaskCanceledException)
            {
                LogInfo($"Sensor data subscription cancelled for device: {deviceMac}");
            }
            catch (Exception ex)
            {
                LogError($"Error in sensor data subscription for device: {deviceMac}", ex);
            }
            finally
            {
                lock (_dataSubscribers)
                {
                    if (_dataSubscribers.ContainsKey(deviceMac))
                    {
                        _dataSubscribers[deviceMac].Remove(responseStream);
                        if (_dataSubscribers[deviceMac].Count == 0)
                            _dataSubscribers.Remove(deviceMac);
                    }
                }
                LogInfo($"Sensor data subscription ended for device: {deviceMac}");
            }
        }

        public override async Task SubscribeToSensorEvents(SubscribeRequest request,
            IServerStreamWriter<SensorEventMessage> responseStream, ServerCallContext context)
        {
            var deviceMac = request.DeviceMac;
//          _logger.LogInformation("Starting sensor event subscription for device: {DeviceMac}", deviceMac);

            lock (_eventSubscribers)
            {
                if (!_eventSubscribers.ContainsKey(deviceMac))
                    _eventSubscribers[deviceMac] = new List<IServerStreamWriter<SensorEventMessage>>();
                _eventSubscribers[deviceMac].Add(responseStream);
            }

            try
            {
                // Keep the stream open until cancellation using infinite delay
                // Using CancellationToken.WaitHandle for .NET Framework compatibility
                var cancelSignal = new ManualResetEventSlim(false);
                using (context.CancellationToken.Register(() => cancelSignal.Set()))
                {
                    cancelSignal.Wait();
                }
            }
            catch (TaskCanceledException)
            {
//              _logger.LogInformation("Sensor event subscription cancelled for device: {DeviceMac}", deviceMac);
            }
            finally
            {
                lock (_eventSubscribers)
                {
                    if (_eventSubscribers.ContainsKey(deviceMac))
                    {
                        _eventSubscribers[deviceMac].Remove(responseStream);
                        if (_eventSubscribers[deviceMac].Count == 0)
                            _eventSubscribers.Remove(deviceMac);
                    }
                }
//              _logger.LogInformation("Sensor event subscription ended for device: {DeviceMac}", deviceMac);
            }
        }

        public async void CloseAll()
        {
            foreach(string device in _deviceDataMapping.Keys)
            {
                bool res = _sensorService.DisConnect(device);
                if (res)
                {
                    await _sensorService.DeviceExit(device);
                }
            }
        }

        private async void OnSensorDataReceived(object sender, List<WCY.DisLab.SDK.Devices.Model.SensorDataModel> data)
        {
            if (data == null || data.Count == 0) return;

            try
            {
                // Find which device this data belongs to
                string deviceMac = FindDeviceMacForData(data);
                if (string.IsNullOrEmpty(deviceMac)) return;

                var message = new SensorDataMessage();
                message.SensorData.AddRange(data.Select(d => new SensorDataModel
                {
                    RangeId = d.RangeId,
                    DataValue = d.DataValue ?? 0
                }));

                //foreach (var d in data)
                //{
                //    LogInfo($"Sensor data for {deviceMac} rangeId: {d.RangeId} value: {d.DataValue}");
                //}

                // Broadcast to all subscribers for this device
                List<IServerStreamWriter<SensorDataMessage>> subscribers;
                lock (_dataSubscribers)
                {
                    if (!_dataSubscribers.TryGetValue(deviceMac, out subscribers) || subscribers.Count == 0)
                        return;
                    // subscribers = subscribers.ToList(); // Create a copy to avoid modification during iteration
                }

                foreach (var subscriber in subscribers)
                {
                    try
                    {
                        await subscriber.WriteAsync(message);

                    }
                    catch (Exception ex)
                    {
                        LogError($"Error sending data to subscriber for device: {deviceMac}", ex);
                    }
                }
            }
            catch (Exception ex)
            {
                LogError("Error processing sensor data", ex);
            }
        }

        private void OnSensorRemoved(object sender, string deviceMac)
        {
            LogInfo($"Sensor removed: {deviceMac}");

            var message = new SensorEventMessage
            {
                DeviceMac = deviceMac,
                EventType = "DeviceRemoved",
                Message = $"Device {deviceMac} disconnected"
            };

            BroadcastEvent(message);
            CleanupDeviceSubscriptions(deviceMac);

            LogInfo($"Stopping data send for device: {deviceMac}");
            _sensorService.StopSendData(deviceMac);
        }

        private async void BroadcastEvent(SensorEventMessage message)
        {
            List<IServerStreamWriter<SensorEventMessage>> subscribers;
            lock (_eventSubscribers)
            {
                if (!_eventSubscribers.TryGetValue(message.DeviceMac, out subscribers) || subscribers.Count == 0)
                    return;
                subscribers = subscribers.ToList();
            }

            var tasks = subscribers.Select(async subscriber =>
            {
                try
                {
                    await subscriber.WriteAsync(message);
                }
                catch (Exception ex)
                {
                    LogError($"Error sending event to subscriber for device: {message.DeviceMac}", ex);
                }
            });

            await Task.WhenAll(tasks);
        }

        private string FindDeviceMacForData(List<WCY.DisLab.SDK.Devices.Model.SensorDataModel> data)
        {
            // This is a simplified implementation - you may need to implement
            // a more sophisticated device-data mapping based on your requirements
            lock (_deviceDataMapping)
            {
                foreach (string device in _deviceDataMapping.Keys)
                {
                    if (_deviceDataMapping[device].Contains(data.FirstOrDefault().RangeId)) return device;
                }

                return "";
            }
        }

        private void CleanupDeviceSubscriptions(string deviceMac)
        {
            lock (_dataSubscribers)
            {
                _dataSubscribers.Remove(deviceMac);
            }
            lock (_eventSubscribers)
            {
                _eventSubscribers.Remove(deviceMac);
            }
            lock (_deviceDataMapping)
            {
                _deviceDataMapping.Remove(deviceMac);
            }
        }

        // Helper methods for mapping between SDK models and gRPC models
        private DeviceModel ToGrpcDeviceModel(WCY.DisLab.SDK.Devices.Model.DeviceModel device)
        {
            return new DeviceModel
            {
                DeviceId = device.DeviceId,
                DeviceName = device.DeviceName,
                DeviceMac = device.DeviceMac,
                DeviceType = (DeviceType)device.DeviceType
            };
        }

        private DataTypeModel ToGrpcDataTypeModel(WCY.DisLab.SDK.Devices.Model.DataTypeModel dataType)
        {
            return new DataTypeModel
            {
                RangeId = dataType.RangeId,
                RangeName = dataType.RangeName ?? string.Empty,
                Unit = dataType.Unit ?? string.Empty,
                LowerLimit = dataType.LowerLimit ?? 0,
                UpperLimit = dataType.UpperLimit ?? 0,
                DecimalCount = dataType.DecimalCount ?? 0
            };
        }

        private WCY.DisLab.SDK.Devices.Model.DataTypeModel FromGrpcDataTypeModel(DataTypeModel dataType)
        {
            return new WCY.DisLab.SDK.Devices.Model.DataTypeModel
            {
                RangeId = dataType.RangeId,
                RangeName = dataType.RangeName,
                Unit = dataType.Unit,
                LowerLimit = dataType.LowerLimit,
                UpperLimit = dataType.UpperLimit,
                DecimalCount = dataType.DecimalCount
            };
        }
    }
}