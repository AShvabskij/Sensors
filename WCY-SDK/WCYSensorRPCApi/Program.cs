using System;
using Microsoft.Extensions.Logging;
using Grpc.Core;
using GrpcServer;
using GrpcServer.Services;

using GrpcSensorServer;
using GrpcSensorServer.Services;
using System.Threading;

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

