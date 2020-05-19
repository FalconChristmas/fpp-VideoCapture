#!/bin/bash

# fpp-VideoCapture uninstall script
echo "Running fpp-VideoCapture uninstall Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make clean

