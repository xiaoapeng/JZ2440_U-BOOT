#!/bin/bash
dd if=u-boot.bin of=u-boot4k.bin bs=1024 count=$1
arm-linux-objdump -D u-boot	>u-boot.dis
sudo oflash 0 1 $2 0 0 u-boot4k.bin 
