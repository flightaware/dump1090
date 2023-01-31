# dump1090-FlightEye
Simple Mode-S decoder with added networking and data filtering

### Instructions to Run: 
In different terminals, run
<ol>
  <li> ./dump1090 --write-json jsondata </li>
  <li> server.py </li>
  <li> client.py </li>
  <li> client.cs (dotnet run) </li>
</ol>

### Instructions for GPS Config: 
<ol>
  <li> Install gpsd, gps-clients </li>
  <li> Check if system uses ttyUSB0 or ttyACM0 by doing ls /dev </li>
  <li> Conigure /etc/default/gpsd to be </li>
  <ol>
      <li> DEVICES = "/dev/ttyACM0" </li>
      <li> USBAUTO = "true" </li>
      <li> GPSD_SOCKET = "/var/run/gpsd.sock" </li>
      <li> GPSD_OPTIONS = "-n -G -b" </li>
   </ol>
<li> Run xgps or cgps to see if gps can get a fix </li>
</ol>

### Instructions for setting up Dump1090: 
<ol>
  <li> sudo apt-get install build-essential fakeroot debhelper librtlsdr-dev pkg-config libncurses5-dev libbladerf-dev libhackrf-dev liblimesuite-dev </li>
  <li> ./prepare-build.sh bullseye    # or buster, or stretch </li>
  <li> cd package-bullseye            # or buster, or stretch </li>
  <li> dpkg-buildpackage -b --no-sign </li>
</ol>

### If GPS reports "GPS Poll Failed" and does not recover
<ol>
  <li> The GPS likely needs to be restarted </li>
  <li> In Linux, do systemctl stop gpsd/gpsd.socket </li>
  <li> Then, do systemctl start gpsd/gpsd.socket </li>
</ol>


