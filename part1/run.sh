#!/bin/bash

echo "Cleanup earlier driver"
rmmod hcsr_driver
dmesg -c

echo "Inserting driver"
insmod hcsr_driver.ko Number_Of_Devices=$(($1))

for((i=0;i<$1;i++))
do
var=$(echo "/dev/hcsr$i")
chmod 777 $var
done
echo "All done"
