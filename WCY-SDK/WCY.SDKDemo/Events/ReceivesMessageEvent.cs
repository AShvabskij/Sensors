using Prism.Events;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WCY.SDKDemo.Models;

namespace WCY.SDKDemo.Events
{
    public class ReceivesMessageEvent:PubSubEvent<LogModel>
    {
    }
}
