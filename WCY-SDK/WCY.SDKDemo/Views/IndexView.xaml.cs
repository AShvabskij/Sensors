using Prism.Events;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;
using WCY.SDKDemo.Events;
using WCY.SDKDemo.Models;

namespace WCY.SDKDemo.Views
{
    /// <summary>
    /// IndexView.xaml
    /// </summary>
    public partial class IndexView : UserControl
    {
        public IndexView(IEventAggregator eventAggregator)
        {
            InitializeComponent();
            eventAggregator.GetEvent<ReceivesMessageEvent>().Subscribe(ReceivesMessageHandler);
        }

        private void ReceivesMessageHandler(LogModel model)
        {
            App.Current.Dispatcher.Invoke(() =>
            {
                Run run = new Run();
                Foreground = new SolidColorBrush(Colors.Black);
                if (model.Message != ""&&model.Message!=null)
                {
                    run.Text=model.Message;
                }
                else
                {
                    run.Text = $"RangeId:{model.RangeId}：DataValue：{model.DataValue}";
                }
                AddLog(run);
            });
        }
        private void AddLog(Run run)
        {
            LineBreak lineBreak = new LineBreak();
            Log.Inlines.Add(run);
            Log.Inlines.Add(lineBreak);
            ScrollViewer.ScrollToEnd();
        }

        private void menuClear_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            Log.Inlines.Clear();
        }
    }
}
