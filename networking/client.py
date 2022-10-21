import socket
import sys
import threading
import json

try:
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
except socket.error:
    print('Failed to create socket')
    sys.exit()
print('Socket created')

#establish connection to server
xr_ip = '10.0.0.166'
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

#send messages to server
while True:
    msg = input('> ')
    client.send(bytes(msg, encoding = 'utf-8'))