using HandyControl.Controls;
using HandyControl.Data;
using Prism.Commands;
using Prism.Events;
using Prism.Ioc;
using Prism.Regions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Documents;
using WCY.DisLab.SDK;
using WCY.DisLab.SDK.Devices.Model;
using WCY.DisLab.SDK.Dictionary.Model;
using WCY.DisLab.SDK.Model;
using WCY.SDKDemo.Events;
using WCY.SDKDemo.Extensions;
using WCY.SDKDemo.Models;
using WCY.SDKDemo.Service;

namespace WCY.SDKDemo.ViewModels
{
    public class SensorInfoViewModel : NavigationViewModel
    {
        private readonly IRegionManager regionManager;
        private ISensorService sensorService = null;
        private readonly IEventAggregator eventAggregator;
        public DelegateCommand<string> ExecuteCommand { get; set; }
        public SDKDemo.Models.SensorDeviceModel DeviceModel { get; set; }
        public List<DataTypeModel> RangeInfos { get; set; }

        private int rangeId;

        public int RangeId
        {
            get { return rangeId; }
            set { SetProperty(ref rangeId, value); }
        }

        private int frequency;

        public int Frequency
        {
            get { return frequency; }
            set { SetProperty(ref frequency, value); }
        }

        private int langType;

        public int LangType
        {
            get { return langType; }
            set { SetProperty(ref langType, value); }
        }


        public SensorInfoViewModel(IContainerProvider containerProvider) : base(containerProvider)
        {
            regionManager = containerProvider.Resolve<IRegionManager>();
            eventAggregator = containerProvider.Resolve<IEventAggregator>();
            ExecuteCommand = new DelegateCommand<string>(Execute);

            this.CloseCommand = new DelegateCommand(() =>
            {
                var region = regionManager.Regions[PrismManager.TabControlRegionName];
                var actionView = region.ActiveViews.FirstOrDefault();
                foreach (var view in region.Views)
                {
                    if (view == actionView)
                    {
                        region.Deactivate(view);
                        region.Remove(actionView);
                    }
                }
            });
        }
        public override void OnNavigatedTo(NavigationContext navigationContext)
        {
            base.OnNavigatedTo(navigationContext);
            string pagtTitle = navigationContext.Parameters.GetValue<string>("pagtTitle");
            this.PageTitle = pagtTitle;
            this.OrderNum = navigationContext.Parameters.GetValue<int>("orderNum");
            DeviceModel = navigationContext.Parameters.GetValue<Models.SensorDeviceModel>("DeviceModel");
            sensorService = navigationContext.Parameters.GetValue<ISensorService>("SensorService");
            sensorService.SensorDataHandler += SensorService_SensorDataHandler;
            sensorService.SensorRemovceHandler += SensorService_SensorRemovceHandler;
        }

        private void SensorService_SensorRemovceHandler(object sender, string e)
        {

        }

        private void SensorService_SensorDataHandler(object sender, List<SensorDataModel> e)
        {
            foreach (var sensor in e)
            {
                LogModel model = new LogModel();
                model.RangeId = sensor.RangeId;
                model.DataValue = (double)sensor.DataValue;
                eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
            }
        }

        private void Execute(string obj)
        {
            switch (obj)
            {
                case "GetRange":
                    GetRange();
                    break;
                case "Start":
                    Start();
                    break;
                case "Stop":
                    Stop();
                    break;
                case "SetLang":
                    SetLang();
                    break;
                case "GetAllRange":
                    GetAllRange();
                    break;
                case "SetRange":
                    SetRange();
                    break;
                case "GetFrequency":
                    GetFrequency();
                    break;
                case "GetSupportFrequency":
                    GetSupportFrequency();
                    break;
                case "SetFrequency":
                    SetFrequency();
                    break;
                case "ClearZero":
                    SetZero();
                    break;
                case "DisConnect":
                    DisConnect();
                    break;
                case "Close":
                    Close();
                    break;
            }
        }

        private async void SetZero()
        {
            if (RangeInfos == null || RangeInfos.Count < 0)
            {
                Growl.Info("请先获取量程信息");
                return;
            }
            bool result = await sensorService.ClearZero(DeviceModel.DeviceMac, RangeInfos);
        }

        private async void SetFrequency()
        {
            if (Frequency == 0)
            {
                Growl.Info("请输入采样频率");
                return;
            }
            bool result = await sensorService.SetFrequency(DeviceModel.DeviceMac, Frequency);
            LogModel model = new LogModel();
            if (result)
            {
                model.Message = $"SetFrequency Successfual";
            }
            else
            {
                model.Message = $"SetFrequency Fail";
            }
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
        }

        private async void GetSupportFrequency()
        {
            List<FrequencyModel> frequencys = await sensorService.GetSupportFrequency(DeviceModel.DeviceMac);
            StringBuilder sb = new StringBuilder();
            sb.Append("Device Frequency is ");
            foreach (var frequency in frequencys)
            {
                sb.Append(frequency.Frequency + ",");
            }
            LogModel model = new LogModel();
            model.Message = sb.ToString().Substring(0, sb.Length - 1);
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
        }

        private async void GetFrequency()
        {
            int frequency = await sensorService.GteCrrentFrequency(DeviceModel.DeviceMac);
            LogModel model = new LogModel();
            model.Message = $"Current frequency：{frequency}";
            eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
        }

        private async void SetRange()
        {
            if (RangeId == 0)
            {
                Growl.Info("请输入量程ID");
                return;
            }
            bool result = await sensorService.SetRange(DeviceModel.DeviceMac, RangeId);
        }

        private async void GetAllRange()
        {
            List<SwitchRangeModel> rangeModels = await sensorService.GetSwitchRange(DeviceModel.DeviceMac);
            if (rangeModels.Count > 0)
            {
                foreach (var range in rangeModels)
                {
                    LogModel model = new LogModel();
                    model.Message = $"RangeId：{range.RangeId}；Value：{range.LowerLimit}-{range.UpperLimit}";
                    eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
                }
            }
        }

        private async void SetLang()
        {
            if (LangType < 0)
            {
                Growl.Info("请输入0-6范围内的语言类型");
                return;
            }
            else if (LangType >= 6)
            {
                Growl.Info("请输入0-6范围内的语言类型");
                return;
            }
            bool result = await sensorService.SetSensorLang(DeviceModel.DeviceMac, LangType);
        }
        private void DisConnect()
        {
            var result = sensorService.DisConnect(DeviceModel.DeviceMac);
        }

        private async void GetRange()
        {
            RangeInfos = await sensorService.GetSensorRange(DeviceModel.DeviceMac);
            if (RangeInfos.Count > 0)
            {
                foreach (var range in RangeInfos)
                {
                    LogModel model = new LogModel();
                    model.Message = $"RangeId:{range.RangeId}；RangeName：{range.RangeName}";
                    eventAggregator.GetEvent<ReceivesMessageEvent>().Publish(model);
                }
            }
        }

        private async void Start()
        {
            if (RangeInfos != null && RangeInfos.Count > 0)
            {
                //sensorService.SensorDataHandler += SensorService_SensorDataHandler;
                bool result = await sensorService.StartSendData(DeviceModel.DeviceMac, RangeInfos);
            }
            else
            {
                Growl.Info("请先获取量程");
                return;
            }
        }

        private async void Stop()
        {
            bool result = await sensorService.StopSendData(DeviceModel.DeviceMac);
        }

        private async void Close()
        {
            bool result=await sensorService.DeviceExit(DeviceModel.DeviceMac);
            //sensorService.SensorClose(DeviceModel);
        }
    }
}
