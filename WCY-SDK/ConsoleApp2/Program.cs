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
            Console.WriteLine("Start USB scanning...");
            List<DeviceModel> usbDevices = await _sensorService.USBScan();
            await proccessDevices(usbDevices);

            Console.WriteLine("Start BLE scanning...");
            List<DeviceModel> bleDevices = await _sensorService.BLEScan(5000);
            await proccessDevices(bleDevices);

            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }

        static async Task proccessDevices(List<DeviceModel> devices)
        {
            if (devices.Count == 0)
            {
                Console.WriteLine("No devices found!");
                ConnectEnum result = await _sensorService.ConnectAsync("00D478BC"); 
                if (result == ConnectEnum.Successful)
                {
                    Console.WriteLine("Successful");
                }

                    
            }
            // Continue with your code that uses the devices
            foreach (var device in devices)
            {
                Console.WriteLine($"Found device: {device.DeviceName}");
                ConnectEnum result = await _sensorService.ConnectAsync(device.DeviceMac);
                if (result == ConnectEnum.Successful)
                {
                    Console.WriteLine("Successful");

                    Console.WriteLine($"Getting sensor range for device: {device.DeviceMac}");
                    var ranges = await _sensorService.GetSensorRange(device.DeviceMac);

                    Console.WriteLine($"Found {ranges.Count} ranges for device {device.DeviceMac}");


                    //                    _sensorService.DisConnect(device.DeviceMac);
                }
                else
                {
                    Console.WriteLine("Failed");
                }

            }

        }
    }
}

