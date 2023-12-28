#!/bin/bash

echo Upload to SD card

read -p "Want to check USB first (y/n)? " readUSB

if [[ $readUSB == "y" || $readSUB == "Y" ]]; then
lsblk
fi

read -p "Type in device (e.g /dev/sd?1): " device

read -p "Proceed (y/n)? " check

if [[ $check == "y" || $check == "Y" ]]; then
sudo dd if=./build/out.bin of=$device conv=fsync bs=4096
else
echo "Abort upload"
fi
