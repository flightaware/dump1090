# dump1090-FlightEye
Simple Mode-S decoder with added networking and data filtering

Instructions to Run: \
In different terminals, run \
1). ./dump1090 --write-json jsondata \
2). server.py \
3). client.py \
4). client.cs (dotnet run) 

Instructions for GPS Config: \
1). Install gpsd, gps-clients \
2). Check if system uses ttyUSB0 or ttyACM0 by doing ls /dev \
3). Conigure /etc/default/gpsd to be \
DEVICES = "/dev/ttyACM0" \
USBAUTO = "true" \
GPSD_SOCKET = "/var/run/gpsd.sock" \
GPSD_OPTIONS = "-n -G -b" \
4). Run xgps or cgps to see if gps can get a fix 

Instructions for setting up Dump1090: \
sudo apt-get install build-essential fakeroot debhelper librtlsdr-dev pkg-config libncurses5-dev libbladerf-dev libhackrf-dev liblimesuite-dev  \ 
./prepare-build.sh bullseye    # or buster, or stretch \ 
cd package-bullseye            # or buster, or stretch \
dpkg-buildpackage -b --no-sign 


