public class Aircraft 
{   
    //* ICAO (for debugging purposes)*//
    public string icao { get; set; }
    //* Barometric Altitude *//
    public float alt_baro { get; set; }
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
    //* Linked List of an Aircrafts ADS-B history *//
    public LinkedList<Aircraft>? history { get; set; } 

    //* DEBUGGING: Delay between message sending to receiving*//
    public TimeSpan delay { get; set; }

    public Aircraft (string icao, float alt_baro, float gs, float track, float lat, float lon, string time)
    {   
        this.icao = icao;
        this.alt_baro = alt_baro;
        this.gs = gs;
        this.track = track;
        this.lat = lat;
        this.lon = lon;
        this.time = DateTime.Parse(time);
        this.delay = DateTime.Now.Subtract(this.time);
        this.history = new LinkedList<Aircraft>();
    }

    public Aircraft (string icao, float alt_baro, float gs, float track, float lat, float lon, string time, Aircraft previous_aircraft, int history_size)
    {   
        this.icao = icao;
        this.alt_baro = alt_baro;
        this.gs = gs;
        this.track = track;
        this.lat = lat;
        this.lon = lon;
        this.time = DateTime.Parse(time);
        this.delay = DateTime.Now.Subtract(this.time);
        this.history = configureLinkedList(previous_aircraft, history_size);
    }

    private LinkedList<Aircraft>? configureLinkedList(Aircraft prev_aircraft, int history_size)
    {   
        if (prev_aircraft.history != null)
        {
            LinkedList<Aircraft> new_history = prev_aircraft.history;
            new_history.AddFirst(prev_aircraft); //add previous aircraft to the new history
            while(new_history.Count > history_size)
            {
                new_history.RemoveLast();
            }
            prev_aircraft.history = null; 
            return new_history;
        }
        return null;
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
    
    /* Debug to print all Aircraft members in the history */
    public void printAircraftHistory()
    {
        if (this.history != null)
        {   
            //make copy of linked list right here
            LinkedList<Aircraft> historyCpy = copyLinkedList(this.history);
            TimeSpan timeDiff = DateTime.Now.Subtract(time);
            Console.WriteLine(icao + " " + alt_baro + "  " + gs + "  " + track + "   " + lat + "  "+ lon  + "  "+ timeDiff + "  " + delay); //print itself
            Console.WriteLine("HISTORY:" + this.history.Count);
            foreach(Aircraft aircraft in historyCpy)
            {
                aircraft.printAircraftData(); //print data for Aircraft in history
            }
            Console.WriteLine("END HISTORY");
        }
    }


    /* Print individual fields of an Aircraft */
    private void printAircraftData()
    {   
        TimeSpan timeDiff = DateTime.Now.Subtract(time);
        Console.WriteLine(icao + " " + alt_baro + "  " + gs + "  " + track + "   " + lat + "  "+ lon  + "  "+ timeDiff + "  " + delay);
    }


}