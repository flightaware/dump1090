import socket
import threading
import time

HOST = '127.0.0.1'#"10.0.0.166"  # Standard loopback interface address (localhost)
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

#send messages recieved from client1, to client2
def client1():
    while True:
        try:
            msgsize = int(conn1.recv(3).decode()) # Receive the incoming JSON message size as int
            msg = bytearray()
            while len(msg) < msgsize :    
                packet = conn1.recv(msgsize - len(msg)) # Receieve the incoming JSON message
                msg.extend(packet)
            
            #print(msgsize, '\n', msg, '\n')
            conn2.sendall(bytes(str(msgsize), encoding = "utf-8")) # Send size message to client2
            conn2.sendall(bytes(msg)) # Send JSON message to client2
        except:
            print('Error receiving message: skipping ', msgsize, ' ', msg)
            continue

#send messages recieved from client2, to client1
def client2():
    while True:
        msg = conn2.recv(1024)
        conn1.sendall(msg)
    
client1_t = threading.Thread(target = client1, daemon= True)
client2_t = threading.Thread(target = client2, daemon= True)

client1_t.start()
client2_t.start()

client1_t.join()
client2_t.join()

    
    