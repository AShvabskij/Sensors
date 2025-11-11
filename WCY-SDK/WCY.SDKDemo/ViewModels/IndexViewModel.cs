using HandyControl.Controls;
using Prism.Commands;
using Prism.Events;
using Prism.Ioc;
using Prism.Regions;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Windows.Threading;
using WCY.DisLab.SDK;
using WCY.DisLab.SDK.Devices.Enum;
using WCY.DisLab.SDK.Devices.Model;
using WCY.DisLab.SDK.Dictionary.Model;
using WCY.DisLab.SDK.Model;
using WCY.SDKDemo.Events;
using WCY.SDKDemo.Models;
using WCY.SDKDemo.Service;

namespace WCY.SDKDemo.ViewModels
{
    public class IndexViewModel : NavigationViewModel
    {
        private readonly Dispatcher dispatcher;
        private readonly IEventAggregator eventAggregator;
        ISensorService SensorService = new SensorService();
        public DelegateCommand<string> ExecuteCommand { get; private set; }
        /// <summary>
        /// Connect
        /// </summary>
        public DelegateCommand<SensorDeviceModel> ConnectCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> GetRangeCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> StartCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> StopCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> SetLangCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> GetAllRangeCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> SetRangeCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> GetFrequecyCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> GetSupportFrequencyCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> SetFrequencyCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> ClearZeroCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> DisConnectCommand { get; set; }
        public DelegateCommand<SensorDeviceModel> CloseDeviceCommand { get; set; }

        private ObservableCollection<SensorDeviceModel> sensorList;
        /// <summary>
        /// Scan device list
        /// </summary>
        public ObservableCollection<SensorDeviceModel> SensorList
        {
            get { return sensorList; }
            set { SetProperty(ref sensorList, value); }
        }

        private ObservableCollection<SensorDeviceModel> tabList;

        public ObservableCollection<SensorDeviceModel> TabList
        {
            get { return tabList; }
            set { SetProperty(ref tabList, value); }
        }

        public IndexViewModel(IContainerProvider containerProvider) : base(containerProvider)
        {
            dispatcher = containerProvider.Resolve<Dispatcher>();
            eventAggregator = containerProvider.Resolve<IEventAggregator>();
            ExecuteCommand = new DelegateCommand<string>(Execute);
            TabList = new ObservableCollection<SensorDeviceModel>();
            ConnectCommand = new DelegateCommand<SensorDeviceModel>(Connect);
            DisConnectCommand = new DelegateCommand<SensorDeviceModel>(DisConnect);
            GetRangeCommand = new DelegateCommand<SensorDeviceModel>(GetRangeInfo);
            StartCommand = new DelegateCommand<SensorDeviceModel>(Start);
            StopCommand = new DelegateCommand<SensorDeviceModel>(Stop);
            SetLangCommand = new DelegateCommand<SensorDeviceModel>(SetLang);
            GetAllRangeCommand = new DelegateCommand<SensorDeviceModel>(GetAllRange);
            SetRangeCommand = new DelegateCommand<SensorDeviceModel>(SetRange);
            GetFrequecyCommand = new DelegateCommand<SensorDeviceModel>(GetFrequecy);
            GetSupportFrequencyCommand = new DelegateCommand<SensorDeviceModel>(GetSupportFrequency);
            SetFrequencyCommand = new DelegateCommand<SensorDeviceModel>(SetFrequency);
            ClearZeroCommand = new DelegateCommand<SensorDeviceModel>(ClearZero);
            CloseDeviceCommand = new DelegateCommand<SensorDeviceModel>(CloseDevice);
        }

        public override void OnNavigatedTo(NavigationContext navigationContext)
        {
            base.OnNavigatedTo(navigationContext);
            SensorList = new ObservableCollection<SensorDeviceModel>();
            SensorService.SensorDataHandler += SensorService_SensorDataHandler;
            SensorService.SensorRemovceHandler += SensorService_SensorRemovceHandler;
            SensorService.Initialize("en");
        }
        private void SensorService_SensorRemovceHandler(object sender, string DeviceMac)
        {
            Console.WriteLine($"The MAC address of the disconnected device is {DeviceMac}");
            var model = TabList.FirstOrDefault(p => p.DeviceMac == DeviceMac);
            if (model != null)
            {
                dispatcher.Invoke(() =>
                {
                    TabList.Remove(model);
                });
                LogModel logmodel = new LogModel();
                logmodel.Message = $"{model.DeviceName} Disconnected";
                eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
            }
        }

        private void SensorService_SensorDataHandler(object sender, List<SensorDataModel> e)
        {
            foreach (var sensor in e)
            {
                Console.WriteLine($"RangeId:{sensor.RangeId}；Data:{sensor.DataValue}");
                LogModel model = new LogModel();
                model.RangeId = sensor.RangeId;
                model.DataValue = (double)sensor.DataValue;
                eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
            }
        }
        #region Connect
        private async void Connect(SensorDeviceModel model)
        {
            ConnectEnum result = await SensorService.ConnectAsync(model.DeviceMac);
            if (result == ConnectEnum.Successful)
            {
                var sensorModel = SensorList.FirstOrDefault(p => p.DeviceMac == model.DeviceMac);
                if (sensorModel != null)
                {
                    SensorList.Remove(sensorModel);
                }
                TabList.Add(new SensorDeviceModel
                {
                    DeviceId = model.DeviceId,
                    DeviceName = model.DeviceName,
                    IsSelected = true,
                    DeviceMac = model.DeviceMac,
                    DeviceType = model.DeviceType,
                    RangeInfos = new List<DataTypeModel>(),
                    LangType = 0,
                    RangeId = 0,
                    Frequency = 0,
                });
                LogModel logmodel = new LogModel();
                logmodel.Message = $"{model.DeviceName}Connected...";
                eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
            }
        }
        #endregion

        #region GetRangeInfo
        private async void GetRangeInfo(SensorDeviceModel model)
        {
            model.RangeInfos = await SensorService.GetSensorRange(model.DeviceMac);
            if (model.RangeInfos.Count > 0)
            {
                foreach (var range in model.RangeInfos)
                {
                    LogModel logmodel = new LogModel();
                    logmodel.Message = $"RangeId:{range.RangeId}；RangeName：{range.RangeName}";
                    eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
                }
            }
            else
            {
                Growl.Info("Acquisition failure");
                return;
            }
        }
        #endregion

        #region Start
        private async void Start(SensorDeviceModel model)
        {
            if (model.RangeInfos != null && model.RangeInfos.Count > 0)
            {
                bool result = await SensorService.StartSendData(model.DeviceMac, model.RangeInfos);
            }
            else
            {
                Growl.Info("Acquire sensor's range data");
                return;
            }
        }
        #endregion

        #region Stop
        private async void Stop(SensorDeviceModel model)
        {
            bool result = await SensorService.StopSendData(model.DeviceMac);

        }
        #endregion

        #region SetLang
        private async void SetLang(SensorDeviceModel model)
        {
            if (model.LangType != 0 || model.LangType != 1)
            {
                Growl.Info("Please enter a valid range of language types");
                return;
            }
            bool result = await SensorService.SetSensorLang(model.DeviceMac, model.LangType);
        }
        #endregion

        #region GetAllRange
        private async void GetAllRange(SensorDeviceModel model)
        {
            model.SwitchRangeModel = await SensorService.GetSwitchRange(model.DeviceMac);
            if (model.SwitchRangeModel.Count > 0)
            {
                foreach (var range in model.SwitchRangeModel)
                {
                    LogModel logmodel = new LogModel();
                    logmodel.Message = $"RangeId：{range.RangeId}；Value：{range.LowerLimit}-{range.UpperLimit}";
                    eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
                }
            }
            else
            {
                Growl.Info("Acquisition failure");
                return;
            }
        }
        #endregion

        #region SetRange
        private async void SetRange(SensorDeviceModel model)
        {
            if (model.RangeId == 0)
            {
                Growl.Info("input range you want to set");
                return;
            }
            bool result = await SensorService.SetRange(model.DeviceMac, model.RangeId);
        }
        #endregion

        #region GetFrequecy
        private async void GetFrequecy(SensorDeviceModel model)
        {
            int frequency = await SensorService.GteCrrentFrequency(model.DeviceMac);
            LogModel logmodel = new LogModel();
            logmodel.Message = $"Current frequency：{frequency}";
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
        }
        #endregion

        #region GetSupportFrequency
        private async void GetSupportFrequency(SensorDeviceModel model)
        {
            List<FrequencyModel> frequencys = await SensorService.GetSupportFrequency(model.DeviceMac);
            StringBuilder sb = new StringBuilder();
            sb.Append("The frequencies currently supported by the device are：");
            foreach (var frequency in frequencys)
            {
                sb.Append(frequency.Frequency + ",");
            }
            LogModel logmodel = new LogModel();
            logmodel.Message = sb.ToString().Substring(0, sb.Length - 1);
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
        }
        #endregion

        #region SetFrequency
        private async void SetFrequency(SensorDeviceModel model)
        {
            if (model.Frequency == 0)
            {
                Growl.Info("Please enter a sampling frequency for a valid range");
                return;
            }
            bool result = await SensorService.SetFrequency(model.DeviceMac, model.Frequency);
            LogModel logmodel = new LogModel();
            if (result)
            {
                logmodel.Message = $"Successfual";
            }
            else
            {
                logmodel.Message = $"Fail";
            }
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(logmodel);
        }
        #endregion

        #region ClearZero
        private async void ClearZero(SensorDeviceModel model)
        {
            if (model.RangeInfos == null || model.RangeInfos.Count < 0)
            {
                Growl.Info("Acquire sensor's range data");
                return;
            }
            bool result = await SensorService.ClearZero(model.DeviceMac, model.RangeInfos);
        }
        #endregion

        #region DisConnect
        private void DisConnect(SensorDeviceModel model)
        {
            bool result = SensorService.DisConnect(model.DeviceMac);
            TabList.Remove(model);
        }

        #endregion

        #region CloseDevice
        private async void CloseDevice(SensorDeviceModel model)
        {
            bool result = await SensorService.DeviceExit(model.DeviceMac);
        }
        #endregion

        #region Search
        private async void Execute(string obj)
        {

            try
            {

                switch (obj)
                {
                    case "BLE":
                        List<DeviceModel> bleList = await SensorService.BLEScan(5000);
                        AddList(bleList);
                        break;
                    case "USB":
                        List<DeviceModel> usbList = await SensorService.USBScan();
                        AddList(usbList);
                        break;
                }
            }
            catch (Exception ex)
            {
                // Log the exception
                return;
            }

        }
        private void AddList(List<DeviceModel> deviceModels)
        {
            if (deviceModels != null && deviceModels.Count > 0)
            {
                foreach (var device in deviceModels)
                {
                    if (!SensorList.Any(p => p.DeviceType == device.DeviceType && p.DeviceId == device.DeviceId))
                    {
                        dispatcher.Invoke(() =>
                        {
                            SensorList.Add(new SensorDeviceModel
                            {
                                DeviceId = device.DeviceId,
                                DeviceName = device.DeviceName,
                                DeviceMac = device.DeviceMac,
                                DeviceType = device.DeviceType
                            });
                        });
                    }

                }
            }
        }
        #endregion
    }
}
