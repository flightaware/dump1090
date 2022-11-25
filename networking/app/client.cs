using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Text.Json;

public class client {

    public static TcpClient tcpClient;
    public static Dictionary <string, Aircraft> aircraftDict = new Dictionary<string, Aircraft>();

    static void Main(string[] args)
    {
        String server_ip = "127.0.0.1"; //"10.0.0.166";
        Int32 server_port = 55555;
        runClient(server_ip, server_port);
    }

    static void parseJson(string str)
    {  
        try {
            JObject json = JObject.Parse(str);
            Aircraft aircraft;
            /* If an Aircraft exists in the dictionary, check for updated data, else, create a new Aircraft and add it to the dictionary */
            if (!aircraftDict.TryGetValue(json.GetValue("hex").ToString(), out aircraft))
            {
                aircraft = new Aircraft(json.GetValue("hex").ToString(), int.Parse(json.GetValue("alt_baro").ToString()), 
                    float.Parse(json.GetValue("gs").ToString()), float.Parse(json.GetValue("track").ToString()), float.Parse(json.GetValue("lat").ToString()), 
                    float.Parse(json.GetValue("lon").ToString()), json.GetValue("seen").ToString());

                aircraftDict.Add(json.GetValue("hex").ToString(), aircraft); //add new Aircraft to dictionary
            }

            else 
            {
                aircraft.update(int.Parse(json.GetValue("alt_baro").ToString()), 
                    float.Parse(json.GetValue("gs").ToString()), float.Parse(json.GetValue("track").ToString()), float.Parse(json.GetValue("lat").ToString()), 
                    float.Parse(json.GetValue("lon").ToString()), json.GetValue("seen").ToString());
            }
            printDictionary();

        }
        catch (Newtonsoft.Json.JsonReaderException es)
        {
            Console.WriteLine("ArgumentException: {0}",es + str);
        }
    }

    static void printDictionary()
    {   
        Console.Clear();
        Console.WriteLine("ACTIVE FLIGHTS------------------------------------------------------- \r");
        foreach (KeyValuePair<String, Aircraft> aircraft in aircraftDict)
        {
            aircraft.Value.printAircraft();
        }
    }

    static void runClient(String server, Int32 port)
    {
        try
            {
                tcpClient = new TcpClient(server, port);

                NetworkStream stream = tcpClient.GetStream();

                Console.WriteLine("Socket Connected to: ", server);

                // Runs reader thread
                void listen()
                {
                    Byte[] message_len = new Byte[3];
                    while (true)
                    {   
                        try{
                            String recv_message = String.Empty;
                            String recv_message_len = String.Empty;

                            // Read in the size of the incoming JSON
                            Int32 bytes_len = stream.Read(message_len, 0, message_len.Length);
                            recv_message_len = System.Text.Encoding.ASCII.GetString(message_len, 0, bytes_len);
                            int size = int.Parse(recv_message_len);

                            // Read in the JSON given the size
                            Int32 bytes = 0;
                            Byte[] message = new Byte[size];
                            while (bytes < size) 
                            {
                                bytes += stream.Read(message, bytes, size - bytes);
                            }
                            recv_message += System.Text.Encoding.ASCII.GetString(message, 0, bytes);
                            // Pass JSON to parse method 
                            parseJson(recv_message);
                        }
                        catch (Exception e)
                        {
                            Console.WriteLine("Error receiving message {0}", e);
                            continue;
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