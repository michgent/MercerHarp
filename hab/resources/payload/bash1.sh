#!/bin/
#run as sudo
python3 -m pip install pynmea2
python3 -m pip install adafruit-blinka
python3 -m pip install adafruit-circuitpython-mcp9808
python3 -m pip install adafruit-circuitpython-bmp3xx

echo  "enable_uart=1" >> /boot/config.txt
raspi-config
#go into interface options
#select serial, somehting about kernel login
#yes to enable, no to other thing




#gpio serial config
#sudo raspi-config
#go into interface options
#select serial, somehting about kernel login
#yes to enable, no to other thing
#sudo nano /boot/config.txt
#add to end "enable_uart=1"