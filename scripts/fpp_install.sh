#!/bin/bash

# fpp-VideoCapture install script

sudo apt-get -y update
sudo apt-get -y install libcamera-dev

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make
