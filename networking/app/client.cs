using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System.Text.Json;

public class client {

    public static TcpClient? tcpClient;
    public static Dictionary <string, Aircraft> aircraftDict = new Dictionary<string, Aircraft>();
    public static Aircraft? userCraft;
    public static int deleteTime = 15;
    public static int maxHistorySize = 5;
    public static volatile bool threadExit;

    static void Main(string[] args)
    {
        String server_ip = "10.0.0.166";
        Int32 server_port = 55555;
        runClient(server_ip, server_port);
    }

    static void parseJson(string str)
    {  
        try {
            JObject json = JObject.Parse(str);
            Aircraft? aircraft;
            JToken? isGPS = "";
            JToken? icao = "";
            JToken? alt_baro = "";
            JToken? gs = "";
            JToken? track = "";
            JToken? lat = "";
            JToken? lon = "";
            JToken? seen = "";

            json.TryGetValue("isGPS", out isGPS);
            if (isGPS != null) {
                
                /* If GPS is false, then JSON string is an Aircraft */
                if (isGPS.ToString().Equals("false")) {

                    /* Must check that the incoming JSON has a hex value */
                    if (json.TryGetValue("hex", out icao))
                    {
                        /* If does not exist in aircraftDict, add it. */
                        if (!aircraftDict.TryGetValue(icao.ToString(), out aircraft))
                        {
                            if(json.TryGetValue("hex", out icao) && json.TryGetValue("alt_baro", out alt_baro) && json.TryGetValue("gs", out gs) && json.TryGetValue("track", out track) 
                                && json.TryGetValue("lat", out lat) && json.TryGetValue("lon", out lon) && json.TryGetValue("seen", out seen))
                            {
                                aircraft = new Aircraft(icao.ToString(), float.Parse(alt_baro.ToString()), 
                                    float.Parse(gs.ToString()), float.Parse(track.ToString()), float.Parse(lat.ToString()), 
                                    float.Parse(lon.ToString()), seen.ToString());

                                aircraftDict.Add(icao.ToString(), aircraft); //add new Aircraft to dictionary
                            }
                        }
                        /*Else, update the previous Aircraft instance in the aircraftDict*/
                        else 
                        {   
                            if(json.TryGetValue("alt_baro", out alt_baro) && json.TryGetValue("gs", out gs) && json.TryGetValue("track", out track) 
                                && json.TryGetValue("lat", out lat) && json.TryGetValue("lon", out lon) && json.TryGetValue("seen", out seen))
                            {   
                                Aircraft new_aircraft = new Aircraft(icao.ToString(), float.Parse(alt_baro.ToString()), 
                                    float.Parse(gs.ToString()), float.Parse(track.ToString()), float.Parse(lat.ToString()), 
                                    float.Parse(lon.ToString()), seen.ToString(), aircraft, maxHistorySize); //create a new Aircraft, and pass in the previous Aircraft instance/Max history amount

                                aircraftDict.Remove(aircraft.icao);
                                aircraftDict.Add(new_aircraft.icao, new_aircraft);
                            }
                        }
                    }
                }
                /* If GPS is NOT false, then it is true, and it must be GPS information */
                else {

                    /* If the userCraft has not been created, create it (this should only be hit at the start of the program) */
                    if (userCraft == null)
                    {
                        if (json.TryGetValue("icao", out icao) && json.TryGetValue("track", out track) && json.TryGetValue("speed", out gs) && 
                            json.TryGetValue("lon", out lon) && json.TryGetValue("lat", out lat) && json.TryGetValue("time", out seen) && json.TryGetValue("alt", out alt_baro))
                        {
                            userCraft = new Aircraft(icao.ToString(), float.Parse(alt_baro.ToString()), float.Parse(gs.ToString()), 
                                float.Parse(track.ToString()), float.Parse(lat.ToString()), float.Parse(lon.ToString()), seen.ToString());  
                        }
                    }
                    /* Else, update the userCraft */
                    else 
                    {
                        if (json.TryGetValue("icao", out icao) && json.TryGetValue("track", out track) && json.TryGetValue("speed", out gs) && 
                            json.TryGetValue("lon", out lon) && json.TryGetValue("lat", out lat) && json.TryGetValue("time", out seen) && json.TryGetValue("alt", out alt_baro))
                        {
                            Aircraft new_aircraft = new Aircraft(icao.ToString(), float.Parse(alt_baro.ToString()), 
                                    float.Parse(gs.ToString()), float.Parse(track.ToString()), float.Parse(lat.ToString()), 
                                    float.Parse(lon.ToString()), seen.ToString(), userCraft, maxHistorySize); //create a new Aircraft, and pass in the previous userCraft instance/Max history amount

                            userCraft = new_aircraft; //make the primary userCraft the new Aircraft instance
                        }
                    }
                }
            }
        }
        catch (Newtonsoft.Json.JsonReaderException es)
        {
            Console.WriteLine("ArgumentException: {0}",es + str);
        }
    }

    //runs checker thread
    static void runChecks()
    {   
        while (threadExit == false) 
        {   
            /*TODO: Remove printing.*/
            Console.Clear();

            /*Print the userCrafts current position + history*/
            Console.WriteLine("USER CRAFT-----------------------------------------------------------------------------------------------");
            Console.WriteLine("ICAO    Alt   GS    Track    Lat        Lon          Last           Delay");
            if (userCraft != null)
            {
                userCraft.printAircraftHistory();
            }

            /*Print the active Aircraft's current position + history*/
            Console.WriteLine("ACTIVE FLIGHTS-------------------------------------------------------------------------------------------");
            Console.WriteLine("ICAO    Alt   GS    Track    Lat        Lon          Last           Delay");
            Dictionary<string, Aircraft> aircraftDictCpy = new Dictionary<string, Aircraft>(aircraftDict);
            foreach (KeyValuePair<String, Aircraft> aircraft in aircraftDictCpy)
            {   
                TimeSpan span = TimeSpan.FromSeconds(deleteTime);
                if (DateTime.Now.Subtract(aircraft.Value.time) > span)
                {
                    aircraftDict.Remove(aircraft.Value.icao);
                }
                else {
                   aircraft.Value.printAircraftHistory(); 
                }
            }
            Thread.Sleep(1000); //sleep for 1 second before printing again
        }
        Console.WriteLine("runChecks exiting...");
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


                            if (recv_message_len.Equals("") || recv_message_len.Equals("000"))
                            {
                                return;
                            }

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

                //runs writer thread
                // void write()
                // {
                //     //Output
                //     //Todo: Modify to only send an output when signal is processed for FPS loss
                //     while (true)
                //     {
                //         Console.WriteLine(">");
                //         String? input = Console.ReadLine();
                //         Byte[] message = System.Text.Encoding.ASCII.GetBytes(input);
                //         stream.Write(message, 0, message.Length);
                //     }
                // }

                Thread listener = new Thread(listen);
                //Thread writer = new Thread(write);
                Thread runCheck = new Thread(runChecks);

                listener.Start();
               // writer.Start();
                runCheck.Start();

                //When listener thread returns, server was closed, we must exit
                listener.Join();
                threadExit = true;
                Console.WriteLine("Exiting.....");
            }
        catch (ArgumentNullException e)
        {
            Console.WriteLine("ArgumentNullException: {0}",e);
        }
    }
}