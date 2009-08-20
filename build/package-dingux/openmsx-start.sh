#!/bin/sh
# Startup script for openMSX, first phase.
# Run this to start openMSX; running the executable directly does not work.
echo -n 1 > /proc/sys/vm/overcommit_memory
export HOME=/usr/local/home
export OPENMSX_SYSTEM_DATA=/usr/local/share/openmsx
exec openvt -c2 -sw /usr/local/bin/openmsx-start2.sh
