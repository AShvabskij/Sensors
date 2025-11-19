using System;
using Microsoft.Extensions.Logging;
using Grpc.Core;
using GrpcServer;
using GrpcServer.Services;

using GrpcSensorServer;
using GrpcSensorServer.Services;
using System.Threading;

using System.Threading.Tasks;



namespace WCYSensorRPCApi
{
    class Program
    {
        const int Port = 5001;

        static void Main(string[] args)
        {
            Console.WriteLine("=== Sensor Service gRPC Server ===");
            Console.WriteLine("Starting server...");

            try
            {

                //var _sensorService = new WCY.DisLab.SDK.SensorService();
                //bool initResult = _sensorService.Initialize("en");
                //Console.WriteLine($"Sensor service initialized: {initResult}");

                //Console.WriteLine("Start BLE scanning...");
                //var devices = await _sensorService.BLEScan(5000);
                //// List<DeviceModel> devices = await _sensorService.USBScan();

                //if (devices.Count == 0)
                //{
                //    Console.WriteLine("No devices found!");
                //}
                //// Continue with your code that uses the devices
                //foreach (var device in devices)
                //{
                //    Console.WriteLine($"Found device: {device.DeviceName}");
                //}

                //                Thread.Sleep(10000);

                // Create and configure the gRPC service
                var sensorService = new SensorGrpcService();


                var server = new Server
                {
                    Services = { 
                        SensorService.BindService(sensorService),
                        Greeter.BindService(new GreeterService())
                     },
                    Ports = { new ServerPort("localhost", Port, ServerCredentials.Insecure) }
                };

                server.Start();

                Console.WriteLine($"✅ Sensor Service gRPC Server is listening on port {Port} ");
                Console.WriteLine("Server is running in background mode.");

                // Просто ждем бесконечно - сервер будет работать пока процесс не будет убит
                Thread.Sleep(Timeout.Infinite);

                //Console.WriteLine("Press 'q' to stop the server...");
                //// Keep the server running until 'q' is pressed
                //while (Console.ReadKey().Key != ConsoleKey.Q)
                //{
                //    Console.WriteLine("\nPress 'q' to stop the server...");
                //}

                Console.WriteLine("\nShutting down server...");
                sensorService.CloseAll();
                server.ShutdownAsync().Wait();
                Console.WriteLine("✅ Server stopped gracefully.");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ Server terminated unexpectedly: {ex}");
            }
        }

        static void Main2(string[] args)
        {
            // Временно закомментируйте запуск сервера
            Console.WriteLine("Сервер готов к настройке...");

            var server = new Server
            {
                Services = { Greeter.BindService(new GreeterService()) },
                Ports = { new ServerPort("localhost", Port, ServerCredentials.Insecure) }
            };

            server.Start();

            Console.WriteLine($"gRPC сервер запущен на localhost:{Port}");
            Console.WriteLine("Нажмите любую клавишу для остановки...");
            Console.ReadKey();

            server.ShutdownAsync().Wait();


            Console.ReadKey();
        }
    }
}

