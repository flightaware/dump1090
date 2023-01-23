import socket
import sys
import threading
import time

HOST = "10.0.0.166"  # Standard loopback interface address (localhost)
PORT = 55555 # Port to listen on (non-privileged ports are > 1023)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()

    #accept connection from first device, send confirmation message
    conn1, addr1 = s.accept()
    print(f"Connected by {addr1}")

    #accept connection from second device, send confirmation message
    conn2, addr2 = s.accept()
    print(f"Connected by {addr2}")

    #tell conn1 that it can begin sending information now that both clients have connected
    conn1.sendall(bytes('b', encoding='utf-8')) 

    #global bool to track if server should shut down
    global exit 
    exit = False

    #send messages recieved from client1, to client2
    def client1():
        global exit
        while True:
            try:
                msgsize = conn1.recv(3).decode() # Receive the incoming JSON message size as int
                #local client disconnection check
                if msgsize == '':
                    conn2.sendall(bytes("000", encoding = 'utf-8')) #tell xr client to exit by sending 000 message
                    exit = True
                    print('Hard shutdown: Local client disconnected')
                    break
                msgsize = int(msgsize)
                msg = bytearray()
                while len(msg) < msgsize :    
                    packet = conn1.recv(msgsize - len(msg)) # Receieve the incoming JSON message
                    msg.extend(packet)
                conn2.sendall(bytes(str(msgsize), encoding = "utf-8")) # Send size message to client2
                conn2.sendall(bytes(msg)) # Send JSON message to client2
            except socket.error:
                print('XR Client Disconnected : Writing')
                conn1.sendall(bytes("stop", encoding = 'utf-8')) #tell local client to stop sending until reconnect
                break

    #send messages recieved from client2, to client1
    def client2():
        while True:
            try:
                msg = conn2.recv(1024)
                if (msg == b''):
                    print('XR Client Disconnected : Reading')
                    break
                conn1.sendall(msg)
            except socket.error:
                print('Socket Error: Reading')

    #reconnect to xr headset client
    def reconnect():
        conn2, addr2 = s.accept()
        print(f"Reconnected to {addr2}")
        conn1.sendall(bytes('strt', encoding = 'utf-8')) #tell local client to start sending again
        return conn2

    #initial thread startup
    client1_t = threading.Thread(target = client1, daemon= True)
    client2_t = threading.Thread(target = client2, daemon= True)
    client1_t.start()
    client2_t.start()

    #restart threads upon XR headset reconnection
    while True:
        client1_t.join()
        client2_t.join()

        #if local client disconnected, exit
        if exit == True:
            sys.exit()

        conn2 = reconnect()

        client1_t = threading.Thread(target = client1, daemon= True)
        client2_t = threading.Thread(target = client2, daemon= True)

        client1_t.start()
        client2_t.start()


        

        
        