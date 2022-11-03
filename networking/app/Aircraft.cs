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
    
    /* Debug to print Aircraft members */
    public void printAircraft()
    {
        Console.WriteLine("AIRCRAFT: " + this.icao + "\nAlt: " + alt_baro + "\ngs: " +  gs + "\ntrack: " + track + "\nlat " + lat + "\nlon " + lon + "\n");
    }


}