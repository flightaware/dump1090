#/bin/bash
# crontab entry
# * * * * * /home/pi/get_cpu_temp.sh

sudo cp /sys/class/thermal/thermal_zone0/temp /run/dump1090-fa/CPUtemp
