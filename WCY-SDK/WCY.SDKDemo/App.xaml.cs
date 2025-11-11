using Prism.DryIoc;
using Prism.Ioc;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using WCY.SDKDemo.Service;
using WCY.SDKDemo.ViewModels;
using WCY.SDKDemo.Views;

namespace WCY.SDKDemo
{
    /// <summary>
    /// 
    /// </summary>
    public partial class App : PrismApplication
    {
        protected override Window CreateShell()
        {
            return Container.Resolve<MainView>();
        }

        protected override void OnInitialized()
        {
            base.OnInitialized();
            var service = App.Current.MainWindow.DataContext as IConfigureService;
            if (service != null)
            {
                service.Configure();
            }
        }

        protected override void RegisterTypes(IContainerRegistry containerRegistry)
        {
            containerRegistry.Register(typeof(Dispatcher), obj => Application.Current.Dispatcher);
            containerRegistry.RegisterForNavigation<IndexView, IndexViewModel>();
        }
    }
}
