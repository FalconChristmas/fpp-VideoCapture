#!/bin/bash
set -e

# fpp-VideoCapture install script

apt-get -y update
apt-get -y install libv4l-dev v4l-utils libavformat-dev libavcodec-dev libswscale-dev

BASEDIR=$(dirname $0)
cd $BASEDIR
cd ..
make

# No restartFlag: the plugin declares FPP_PLUGIN_SUPPORTS_UNLOAD and the Plugin
# Manager asks fppd to load it as soon as this script finishes, so asking the
# user to restart would interrupt a running show for nothing.
