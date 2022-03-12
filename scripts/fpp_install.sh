#!/bin/bash

# fpp-VideoCapture install script

sudo apt-get -y update
sudo apt-get -y install libcamera-dev libv4l-dev v4l-utils

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make
