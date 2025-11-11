using Prism.Mvvm;
using System.Collections.Generic;
using WCY.DisLab.SDK.Devices.Enum;
using WCY.DisLab.SDK.Devices.Model;
using WCY.DisLab.SDK.Model;

namespace WCY.SDKDemo.Models
{
    public class SensorDeviceModel : BindableBase
    {
        /// <summary>
        /// Device id
        /// </summary>
        public string DeviceId { get; set; }

        private string deviceName;
        /// <summary>
        /// Device name
        /// </summary>
        public string DeviceName
        {
            get { return deviceName; }
            set { SetProperty(ref deviceName, value); }
        }

        private bool isSelected;
        /// <summary>
        /// Check or not
        /// </summary>
        public bool IsSelected
        {
            get { return isSelected; }
            set { SetProperty(ref isSelected, value); }
        }
        /// <summary>
        /// Device mac
        /// </summary>
        public string DeviceMac { get; set; }
        /// <summary>
        /// Device type
        /// </summary>
        public DeviceType DeviceType { get; set; }
        /// <summary>
        /// Range info
        /// </summary>
        public List<DataTypeModel> RangeInfos { get; set; }

        public List<SwitchRangeModel> SwitchRangeModel { get; set; }

        private int langType;
        /// <summary>
        /// Language type
        /// </summary>
        public int LangType
        {
            get { return langType; }
            set { SetProperty(ref langType, value); }
        }


        private int rangeId;
        /// <summary>
        /// Range id
        /// </summary>
        public int RangeId
        {
            get { return rangeId; }
            set { SetProperty(ref rangeId, value); }
        }

        private int frequency;
        /// <summary>
        /// Sampling frequency
        /// </summary>
        public int Frequency
        {
            get { return frequency; }
            set { SetProperty(ref frequency, value); }
        }


    }
}
