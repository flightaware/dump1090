using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

public class client {
    static void Main(string[] args)
    {
        runClient();
    }

    static void runClient()
    {   
        var client = new UdpClient();
        IPAddress ipAddr = IPAddress.Parse("10.0.0.166");
        IPEndPoint endPoint = new IPEndPoint(ipAddr, 55555);

        Console.WriteLine("connecting to rendezvous server");

        try {

            /* Send rendezvous server greeting */
            client.Connect(endPoint);
            client.Send(new byte[] {0}, 1);
            
            /*Establish a received connection with rendezvous server*/
            while (true) 
            {
                Byte [] data = client.Receive(ref endPoint);

                String sdata = Encoding.ASCII.GetString(data).Trim();
                if (string.Equals(sdata, "ready"))
                {
                    Console.WriteLine("checked in with server, waiting");
                    break;
                }
            }

            /* Parse server message for peer ip and port information */
            Byte [] peer = client.Receive(ref endPoint);
            String [] speer = Encoding.ASCII.GetString(peer).Trim().Split(' ');
            String ip = speer[0];
            int sport = Int32.Parse(speer[1]);
            int dport = Int32.Parse(speer[2]);
            
            /* Print peer info */
            Console.WriteLine("\ngot peer");
            Console.WriteLine("  ip:            " + ip);
            Console.WriteLine("  source port:   " + sport);
            Console.WriteLine("  dest port:     " + dport);

            /* Modify variables to connect to our peer */
            // var peerClient = new UdpClient();
            // IPAddress peerIPAddr = IPAddress.Parse(ip);
            // IPEndPoint peerEndPoint = new IPEndPoint(peerIPAddr, dport);

            // /* Create new connection client using the peers endpoint */
            // peerClient.Connect(peerEndPoint);
            // peerClient.Send(new byte[] {0}, 1);

            Console.WriteLine("ready to exchange messages\n");


            /* Reading thread */
                void listen()
                {   
                    Socket sock = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                    IPEndPoint iep = new IPEndPoint(IPAddress.Any, sport);
                    sock.Bind(iep);
                    EndPoint ep = (EndPoint)iep;
                    byte [] threadData = new byte[1024];
                    Console.WriteLine(sock + " " + iep);
                    while(true)
                    {   
                        // threadData = peerClient.Receive(ref peerEndPoint);
                        // String sThreadData = Encoding.ASCII.GetString(threadData).Trim();
                        // Console.WriteLine(sThreadData); 
                        int recv = sock.ReceiveFrom(threadData, ref ep);
                        String sThreadData = Encoding.ASCII.GetString(threadData, 0, recv).Trim();
                        Console.WriteLine(sThreadData);
                    }
                }

            Thread listener = new Thread(listen);
            listener.Start();

            // while (true) 
            // {
            //     Console.WriteLine("> ");
            //     var msg = Console.ReadLine();
            //     if (msg != null)
            //     {
            //         byte [] bmsg = System.Text.Encoding.ASCII.GetBytes(msg);
            //         peerClient.Send(bmsg);
            //     }
            // }
        }

        catch (Exception e) 
        {
            Console.WriteLine(e);
        }
    }
}