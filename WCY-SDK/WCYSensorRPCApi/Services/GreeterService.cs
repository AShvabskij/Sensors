using System;
using System.Threading.Tasks;
using Grpc.Core;

namespace GrpcServer.Services
{
    public class GreeterService : Greeter.GreeterBase
    {
        // Простой RPC метод
        public override Task<HelloReply> SayHello(HelloRequest request, ServerCallContext context)
        {
            return Task.FromResult(new HelloReply
            {
                Message = $"Привет, {request.Name}! Время: {DateTime.Now:HH:mm:ss}"
            });
        }

        // Стриминговый метод
        public override async Task SayHelloStream(HelloRequest request,
            IServerStreamWriter<HelloReply> responseStream, ServerCallContext context)
        {
            for (int i = 1; i <= 5; i++)
            {
                if (context.CancellationToken.IsCancellationRequested)
                    break;

                await responseStream.WriteAsync(new HelloReply
                {
                    Message = $"Сообщение #{i} для {request.Name}"
                });

                await Task.Delay(2000);
            }
        }
    }
}