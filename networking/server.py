import socket
import sys
import threading

HOST = "10.0.0.166"  # Standard loopback interface address (localhost)
PORT = 55555 # Port to listen on (non-privileged ports are > 1023)

#global bool to track if server should shut down
global exit
exit = False

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:

    #Recieves a msgsize amount of bytes from recv_client and forwards it to send_client
    def receive_and_send(recv_client:socket, send_client:socket, msgsize):
        msgsize = int(msgsize)
        msg = bytearray()
        while len(msg) < msgsize :    
            packet = recv_client.recv(msgsize - len(msg)) # Receieve the incoming message from recv_client
            msg.extend(packet)
        send_client.sendall(bytes(str(msgsize), encoding = "utf-8")) # Send message size to send_client
        send_client.sendall(bytes(msg)) # Send message to send_client

    #Send messages recieved from local_client to xr_client
    def handle_local_client_thread():
        global exit
        while True:
            try:
                msgsize = local_client.recv(3).decode() # Receive the incoming JSON message size as int
                #local client disconnect check
                if msgsize == '':
                    xr_client.sendall(bytes("000", encoding = 'utf-8')) #tell xr_client to exit by sending 000 message
                    exit = True
                    print('Hard shutdown: Local client disconnected')
                    break
                receive_and_send(local_client, xr_client, msgsize)
            except socket.error:
                print('XR Client Disconnected : Writing')
                local_client.sendall(bytes("stop", encoding = 'utf-8')) #tell local client to stop sending until reconnect
                break

    #Send messages recieved from xr_client to local_client
    def handle_xr_client_thread():
        while True:
            try:
                msg = xr_client.recv(1024)
                if (msg == b''):
                    print('XR Client Disconnected : Reading')
                    break
                local_client.sendall(msg)
            except socket.error:
                print('Socket Error: Reading')

    #reconnect to xr headset client
    def reconnect_to_xr_client():
        xr_client, xr_client_addr2 = s.accept()
        print(f"Reconnected to {xr_client_addr2}")
        local_client.sendall(bytes('strt', encoding = 'utf-8')) #tell local client to start sending again
        return xr_client

    if __name__ == '__main__':
        #CREATE INITIAL CONNECTION
        s.bind((HOST, PORT))
        s.listen()

        local_client, local_client_addr = s.accept()
        print(f"Connected by {local_client_addr}")

        xr_client, xr_client_addr2 = s.accept()
        print(f"Connected by {xr_client_addr2}")

        #tell local_client that it can begin sending information now that both clients have connected
        local_client.sendall(bytes('b', encoding='utf-8')) 

        #HANDLE THREAD CREATION AND RECREATION (UPON XR CLIENT DISCONNECT)
        local_client_thread = threading.Thread(target = handle_local_client_thread, daemon= True)
        xr_client_thread = threading.Thread(target = handle_xr_client_thread, daemon= True)
        local_client_thread.start()
        xr_client_thread.start()
        while True:
            local_client_thread.join()
            xr_client_thread.join()
            print('Open for Reconnection')
            #if local client disconnected, exit
            if exit == True:
                sys.exit()
            xr_client = reconnect_to_xr_client()
            local_client_thread = threading.Thread(target = handle_local_client_thread, daemon= True)
            xr_client_thread = threading.Thread(target = handle_xr_client_thread, daemon= True)
            local_client_thread.start()
            xr_client_thread.start()
            


        

        
        