using Prism.Regions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WCY.SDKDemo.Extensions;
using WCY.SDKDemo.Service;

namespace WCY.SDKDemo.ViewModels
{
    public class MainViewModel : IConfigureService
    {
        private readonly IRegionManager regionManager;
        public MainViewModel(IRegionManager regionManager)
        {
            this.regionManager = regionManager;
        }
        public void Configure()
        {
            this.regionManager.Regions[PrismManager.MainViewRegionName].RequestNavigate("IndexView");
        }
    }
}
