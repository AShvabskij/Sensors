using Prism.Commands;
using Prism.Ioc;
using Prism.Mvvm;
using Prism.Regions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WCY.SDKDemo.Extensions;

namespace WCY.SDKDemo.Service
{
    public class NavigationViewModel : BindableBase, INavigationAware
    {
        // 页面标题
        private string pageTitle;

        public string PageTitle
        {
            get { return pageTitle; }
            set { SetProperty(ref pageTitle, value); }
        }

        // 是否允许关闭
        public bool IsCanClose { get; set; } = true;

        public int OrderNum { get; set; } = 0;

        public virtual DelegateCommand CloseCommand { get; set; }

        private readonly IContainerProvider containerProvider;
        private readonly IRegionManager regionManager;
        public NavigationViewModel(IContainerProvider containerProvider)
        {
            this.containerProvider = containerProvider;
            this.regionManager = containerProvider.Resolve<IRegionManager>();
        }

        public virtual bool IsNavigationTarget(NavigationContext navigationContext)
        {
            return true;
        }

        public virtual void OnNavigatedFrom(NavigationContext navigationContext)
        {

        }

        public virtual void OnNavigatedTo(NavigationContext navigationContext)
        {

        }
    }
}
