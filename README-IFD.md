Default path to dump1090 service is: /data/flight/
Copy the whole dump1090 directory into it, so the starting scritp could be found in following path:
/data/flight/dump1090/dump1090.sh

dump1090 requires following library to be installed:
apt install libbladerf1

MongoC/BSON libs version required:
mongo-c-driver-1.12.0

In order to use the mongoc static libraries:
`git clone https://github.com/mongodb/mongo-c-driver`
`cd mongo-c-driver`
`cmake .`
`make`
`sudo make install`
`export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/`
And then run `make clean && make` on the dump1090 folder

Create a file /etc/systemd/system/dump1090.service with following content:
'''
[Unit]
After=network.target

[Service]
ExecStart=/data/flight/dump1090/dump1090.sh

[Install]
WantedBy=default.target
'''

Enabling/disabling autostart of the dump1090 service:
systemctl enable|disable dump1090   (it creates/removes a link /etc/systemd/system/default.target.wants/dump1090.service)

Starting/stopping/checking status of the dump1090 service:
systemctl start|stop|status dump1090
