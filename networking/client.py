import socket
import sys
import threading
import json
import time
import haversine as hs
from haversine import Unit
from datetime import datetime
from gps import *

#global filtering distance (miles)
filtering_distance_miles = 1000

#current lat and lon of headset
cur_location = (38.895616, -77.044122) #hardcoded for now (wash DC)

#configure gpsd
gpsd = gps(mode=WATCH_ENABLE|WATCH_NEWSTYLE)

#serve IP and port
ip = '10.0.0.166'
port = 55555

#if xr client disconnects, a lock on sending new data will start until the client reconnects
global clientLock
clientLock = False


#thread to listen for incoming messages
def listen():
    global clientLock
    while True:
        try:
            data = client.recv(4).decode()
            if data == 'stop':
                clientLock = True
            elif data == 'strt':
                clientLock = False
        except socket.error:
            print('Failed to recieve data')

#sends json data over the 
def send_json(json_str):
    msgsize = len(json_str)
    # msgsize must be 3 digits long to fit server protocol, any very short or very long JSON strings are dropped
    if msgsize < 1000 or msgsize > 99:
        print('Sending ' + json_str + '\n')
        client.sendall(bytes(str(msgsize), encoding= 'utf-8')) # send message size prior to sending json (assuming the message size is 3 digits)
        client.sendall(bytes(json_str, encoding = 'utf-8')) # send JSON message
    else:
        print("JSON String Too Large or Small: not sending string")

#Polls GPS data and sends through socket
def send_gps_data():
        nx = gpsd.next()
                
        if nx['class'] == 'TPV':
            altitude = getattr(nx, 'altHAE', "Unknown")
            track = getattr(nx, 'track', "Unknown")
            speed = getattr(nx, 'speed', "Unknown")
            latitude = getattr(nx,'lat', "Unknown")
            longitude = getattr(nx,'lon', "Unknown")
            climb = getattr(nx, 'climb', 'Unknown')
            time = str(datetime.now()).split()[1]
            icao = 'USERCRAFT'

            gps_dict = dict({'alt': altitude, 'track': track, 'speed':speed, 'lon':longitude, 'lat': latitude, 'climb': climb, 'time': time, 'icao': icao, 'isGPS':'true'})
            json_gps = json.dumps(gps_dict)
            send_json(json_gps)
            gpsd.next()
        else:
            gpsd.next()
            print('GPS Poll Failed')

#Controls the GPS polling thread
def run_gps_thread():
    global clientLock

    while True:
        while clientLock: pass
        send_gps_data()
        time.sleep(1.0)

#Parses aircraft JSON data and sends valid aircraft through socket
def send_aircraft_data(path):
    f = open(path, 'r+')
    f_json = json.load(f)
    for aircraft in f_json['aircraft']:
        #data has been updated within last second and lat and lon exists
        if aircraft['seen'] < 1.0 and 'lat' in aircraft and 'lon' in aircraft:
            aircraft_location = (aircraft['lat'], aircraft['lon'])
            rel_dist_miles = hs.haversine(cur_location, aircraft_location, unit = Unit.MILES)
            
            #airplane is within maximum filtering distance
            if rel_dist_miles < filtering_distance_miles:
                aircraft['seen'] = str(datetime.now()).split()[1] #retrieves active time of sending
                aircraft.update({'isGPS':'false'})
                j_aircraft = json.dumps(aircraft)
                send_json(j_aircraft)
    f.close()

#Controls aircraft JSON parsing thread
def run_aircraft_thread():
    while True:
        #if XR client has disconnected, lock loop until client reconnects
        while clientLock: pass

        opentime = datetime.now()
        send_aircraft_data("../dump1090/jsondata/aircraft.json")
        #delay next read for a second (takes into account time it took for last read: 1.0 - time to read last)
        if (datetime.now() - opentime).total_seconds() < 1.0:
            time.sleep(1.0 - (datetime.now() - opentime).total_seconds())

if __name__ == '__main__':
    #Connect to server socket
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    except socket.error:
        print('Failed to create socket')
        sys.exit()
    client.connect((ip, port))
    print('Socket Connected to ' + ip)
    client.recv(1) # wait for server confirmation to begin sending data

    #Start listener thread
    listener = threading.Thread(target = listen)
    listener.start()

    #Start GPS Poller thread
    gpsPoller = threading.Thread(target = run_gps_thread)
    gpsPoller.start()

    run_aircraft_thread()