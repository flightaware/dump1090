import socket
import threading

HOST = "10.0.0.166"  # Standard loopback interface address (localhost)
PORT = 55555 # Port to listen on (non-privileged ports are > 1023)

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()

    #accept connection from first device, send confirmation message
    conn1, addr1 = s.accept()
    print(f"Connected by {addr1}")
    conn1.sendall(bytes("Server connection confirmation, waiting on second client\0", encoding='utf-8'))

    #accept connection from second device, send confirmation message
    conn2, addr2 = s.accept()
    print(f"Connected by {addr2}")
    conn2.sendall(bytes("Server connection confirmation\0", encoding = 'utf-8'))

#send messages recieved from client1, to client2
def client1():
    while True:
        msg = conn1.recv(1024)
        conn2.sendall(msg)

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

    
    