using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

public class client {

    static void Main(string[] args)
    {
        String server_ip = "10.0.0.166";
        Int32 server_port = 55555;
        runClient(server_ip, server_port);
    }

    static void runClient(String server, Int32 port)
    {
        try
            {
                TcpClient client = new TcpClient(server, port);

                NetworkStream stream = client.GetStream();

                Console.WriteLine("Socket Connected to: ", server);

                // Runs reader thread
                // Todo: Modify the input to send data to C# objects
                void listen()
                {
                    Byte[] message = new Byte[1024];
                    while (true)
                    {
                        String recv_message = String.Empty;
                        Int32 bytes = stream.Read(message, 0, message.Length);
                        recv_message = System.Text.Encoding.ASCII.GetString(message, 0, bytes);
                        if(!recv_message.Equals("")) 
                        {
                            Console.WriteLine(recv_message);
                        }
                    }
                }

                Thread listener = new Thread(listen);
                listener.Start();

                //Output
                //Todo: Modify to only send an output when signal is processed for FPS loss
                while (true)
                {
                    Console.WriteLine(">");
                    String input = Console.ReadLine();
                    Byte[] message = System.Text.Encoding.ASCII.GetBytes(input);
                    stream.Write(message, 0, message.Length);
                }
            }
        catch (ArgumentNullException e)
        {
            Console.WriteLine("ArgumentNullException: {0}",e);
        }
    }
}