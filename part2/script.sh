#!/bin/bash

#echo "Cleanup earlier driver"
#rmmod platform_driver.ko
#rmmod platform_device.ko
#dmesg -c

echo "Inserting driver"
insmod platform_device.ko Number_Of_Devices=$(($1))
insmod platform_driver.ko 

for((i=0;i<$1;i++))
do

echo -n "Enter trigger pin for hcsr$i -> "
read trigger_pin
echo "$trigger_pin" > /sys/class/HCSR/hcsr$i/trigger_pin

echo -n "Enter echo pin  for hcsr$i -> "
read echo_pin
echo "$echo_pin" > /sys/class/HCSR/hcsr$i/echo_pin

echo -n "Enter number of samples for hcsr$i -> "
read number_samples
echo "$number_samples" > /sys/class/HCSR/hcsr$i/number_samples

echo -n "Enter sampling period for hcsr$i -> "
read sampling_period
echo "$sampling_period" > /sys/class/HCSR/hcsr$i/sampling_period

echo -n "Enter enable pin for hcsr$i -> "
read enable
echo "$enable" > /sys/class/HCSR/hcsr$i/enable

echo -n "Enable is $enable "

sleep 5

echo -n "latest distance measured for hcsr$i -> " 
cat /sys/class/HCSR/hcsr$i/distance
echo "cm"

done

echo -n "All done"
