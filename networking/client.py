import socket
import sys
import threading
import json
import time
import haversine as hs
from haversine import Unit
from datetime import datetime

#global filtering distance (miles)
filtering_distance_miles = 50
#current lat and lon of headset
cur_location = (33.482342, -112.364749) #hardcoded for now

try:
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
except socket.error:
    print('Failed to create socket')
    sys.exit()
print('Socket created')

#establish connection to server
ip = '127.0.0.1'#'10.0.0.166'
port = 55555
client.connect((ip, port))
print('Socket Connected to ' + ip)
client.recv(1) # wait for server confirmation to begin sending data

#if xr client disconnects, a lock on sending new data will start until the client reconnects
global lock
lock = False

#thread to listen for incoming messages
def listen():
    global lock
    print('ready to listen')
    while True:
        try:
            data = client.recv(4).decode()
            if data == 'stop':
                lock = True
            elif data == 'strt':
                lock = False
        except socket.error:
            print('Failed to recieve data')

listener = threading.Thread(target = listen)
listener.start()


#read from dump1090 json and send valid data to server
while True:
    #if XR client has disconnected, suspend any new messages until told so
    while lock: pass

    f = open("../dump1090/jsondata/aircraft.json", 'r+')
    f_json = json.load(f)
    opentime = datetime.now()

    for aircraft in f_json['aircraft']:
        #data has been updated within last second and lat and lon exists
        if aircraft['seen'] < 1.0 and 'lat' in aircraft and 'lon' in aircraft:
            aircraft_location = (aircraft['lat'], aircraft['lon'])
            rel_dist_miles = hs.haversine(cur_location, aircraft_location, unit = Unit.MILES)
            
            #airplane is within maximum filtering distance
            if rel_dist_miles < filtering_distance_miles:
                aircraft['seen'] = str(datetime.now()).split()[1] #retrieves active time of sending
                j_aircraft = json.dumps(aircraft)
                msgsize = len(j_aircraft)
                # msgsize must be 3 digits long to fit server protocol, any very short or very long JSON strings are dropped
                if msgsize < 1000 or msgsize > 99:
                    client.sendall(bytes(str(msgsize), encoding= 'utf-8')) # send message size prior to sending json (assuming the message size is 3 digits)
                    client.sendall(bytes(j_aircraft, encoding = 'utf-8')) # send JSON message
                else:
                    print("JSON String Too Large: not sending string")
    f.close()
    #delay next read for a second (takes into account time it took for last read: 1.0 - time to read last)
    if (datetime.now() - opentime).total_seconds() < 1.0:
        time.sleep(1.0 - (datetime.now() - opentime).total_seconds())