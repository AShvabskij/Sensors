using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using WCY.DisLab.SDK;
using WCY.DisLab.SDK.Devices.Enum;
using WCY.DisLab.SDK.Devices.Model;
using WCY.DisLab.SDK.Dictionary.Model;
using WCY.DisLab.SDK.Model;

namespace ConsoleApp2
{
    class Program
    {
        private static ISensorService _sensorService = new SensorService();

        static async Task Main(string[] args)
        {
            _sensorService.Initialize("en");

            // List<DeviceModel> devices = await _sensorService.BLEScan(10000);
            List<DeviceModel> devices = await _sensorService.USBScan();

            // Continue with your code that uses the devices
            foreach (var device in devices)
            {
                Console.WriteLine($"Found device: {device.DeviceName}");
                ConnectEnum result = await _sensorService.ConnectAsync(device.DeviceMac);
                if (result == ConnectEnum.Successful)
                {
                    Console.WriteLine("Successful");
                    _sensorService.DisConnect(device.DeviceMac);
                }

            }

            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}

