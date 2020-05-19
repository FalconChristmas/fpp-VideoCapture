#!/bin/sh

echo "Running fpp-VideoCapture PreStart Script"

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make
