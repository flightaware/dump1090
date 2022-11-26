public class Aircraft 
{   
    //* ICAO (for debugging purposes)*//
    public string icao { get; set; }
    //* Barometric Altitude *//
    public int alt_baro { get; set; }
    //* Ground Speed *//
    public float gs { get; set; }
    //* Heading + Drift *//
    public float track { get; set; }
    //* Latitude *//
    public float lat { get; set; }
    //* Longitude *//
    public float lon { get; set; }
    //* Last time data was updated *//
    public DateTime time { get; set; }
    //* DEBUGGING: Delay between message sending to receiving*//
    public TimeSpan delay { get; set; }

    public Aircraft (string icao, int alt_baro, float gs, float track, float lat, float lon, string time)
    {   
        this.icao = icao;
        this.alt_baro = alt_baro;
        this.gs = gs;
        this.track = track;
        this.lat = lat;
        this.lon = lon;
        this.time = DateTime.Parse(time);
        this.delay = DateTime.Now.Subtract(this.time);
    }

    /* Updates Aircraft data based on new ADS-B information */
    public void update(int alt_baro, float gs, float track, float lat, float lon, string time)
    {
        bool update = false;
        if (alt_baro != this.alt_baro)
        {   
            this.alt_baro = alt_baro;
            update = true;
        }

        if (gs != this.gs)
        {
            this.gs = gs;
            update = true;
        }

        if (track != this.track)
        {
            this.track = track;
            update = true;
        }

        if (lat != this.lat)
        {
            this.lat = lat;
            update = true;
        }

        if (lon != this.lon)
        {
            this.lon = lon;
            update = true;
        }

        if (update)
        {
            this.time = DateTime.Parse(time);
            this.delay =  DateTime.Now.Subtract(this.time);
        }
    }
    
    /* Debug to print Aircraft members */
    public void printAircraft()
    {
        TimeSpan timeDiff = DateTime.Now.Subtract(time);
        //Console.WriteLine("AIRCRAFT: " + icao + " Alt: " + alt_baro + " gs: " +  gs + " track: " + track 
        //    + " lat " + lat + " lon " + lon + " last " + timeDiff + " delay " + delay);
        Console.WriteLine(icao + " " + alt_baro + "  " + gs + "  " + track + "   " + lat + "  "+ lon  + "  "+ timeDiff + "  " + delay);
    }


}