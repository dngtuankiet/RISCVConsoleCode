#!/bin/bash

sudo dd if=./build/out.bin of=/dev/sde1 conv=fsync bs=4096