import socket
import sys
import threading
import json
import time
import haversine as hs
from haversine import Unit

#global filtering distance (miles)
filtering_distance_miles = 50
#current lat and lon of headset
cur_location = (38.9072, -77.0369) #hardcoded for now

try:
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
except socket.error:
    print('Failed to create socket')
    sys.exit()
print('Socket created')

#establish connection to server
xr_ip = '10.0.0.48'
xr_port = 55555
client.connect((xr_ip, xr_port))
print('Socket Connected to ' + xr_ip)

#thread to listen for incoming messages
def listen():
    print('ready to listen')
    while True:
        try:
            data = client.recv(1024).decode()
            if data != '':
                print(data)
        except socket.error:
            print('Failed to recieve data')

listener = threading.Thread(target = listen)
listener.start()


#read from dump1090 json and send valid data to server
while True:
    f = open("../dump1090/jsondata/aircraft.json")
    f_json = json.load(f)

    for aircraft in f_json['aircraft']:
        #data has been updated within last second and lat and lon exists
        if aircraft['seen'] <= 1 and 'lat' in aircraft and 'lon' in aircraft:
            aircraft_location = (aircraft['lat'], aircraft['lon'])
            rel_dist_miles = hs.haversine(cur_location, aircraft_location, unit = Unit.MILES)
            #airplane is within maximum filtering distance
            if rel_dist_miles < filtering_distance_miles:
                j_aircraft = json.dumps(aircraft)
                client.sendall(bytes(j_aircraft, encoding = 'utf-8'))

    f.close()