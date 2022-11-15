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

    public Aircraft (string icao, int alt_baro, float gs, float track, float lat, float lon)
    {   
        this.icao = icao;
        this.alt_baro = alt_baro;
        this.gs = gs;
        this.track = track;
        this.lat = lat;
        this.lon = lon;

    }

    /* Updates Aircraft data based on new ADS-B information */
    public void update(int alt_baro, float gs, float track, float lat, float lon)
    {
        if (alt_baro != this.alt_baro)
        {   
            this.alt_baro = alt_baro;
        }

        if (gs != this.gs)
        {
            this.gs = gs;
        }

        if (track != this.track)
        {
            this.track = track;
        }

        if (lat != this.lat)
        {
            this.lat = lat;
        }

        if (lon != this.lon)
        {
            this.lon = lon;
        }

    }
    
    /* Debug to print Aircraft members */
    public void printAircraft()
    {
        Console.WriteLine("AIRCRAFT: " + this.icao + " Alt: " + alt_baro + " gs: " +  gs + " track: " + track + " lat " + lat + " lon " + lon + "\n");
    }


}