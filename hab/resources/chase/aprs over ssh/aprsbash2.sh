#!/bin/bash
#run from root/


# Install dependencies
sudo apt-get update
sudo apt-get -y install git cmake build-essential libusb-1.0 qt4-qmake libpulse-dev libx11-dev

mkdir /home/pi/rtl
cd /home/pi/rtl

# Fetch and compile rtl-sdr source
mkdir -p ~/src/
cd ~/src/
git clone git://git.osmocom.org/rtl-sdr.git
cd rtl-sdr
mkdir build
cd build
cmake ../ -DINSTALL_UDEV_RULES=ON
make
sudo make install
sudo ldconfig

echo "blacklist dvb_usb_rtl28xxu" >>  /etc/modprobe.d/blacklist-rtl.conf
echo "blacklist rtl2832 >>  /etc/modprobe.d/blacklist-rtl.conf
echo "blacklist rtl2830 >>  /etc/modprobe.d/blacklist-rtl.conf

# Fetch and compile multimonNG
cd ~/src/
git clone https://github.com/EliasOenal/multimonNG.git
cd multimonNG
mkdir build
cd build
qmake ../multimon-ng.pro
make
sudo make install


reboot now