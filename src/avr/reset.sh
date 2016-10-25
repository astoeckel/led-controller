#!/bin/bash -e

echo 25 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio25/direction
echo 1 > /sys/class/gpio/gpio25/value
echo in > /sys/class/gpio/gpio25/direction
echo 25 > /sys/class/gpio/unexport

