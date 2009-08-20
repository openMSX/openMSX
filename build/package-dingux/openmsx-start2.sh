#!/bin/sh
# Startup script for openMSX, second phase.
# To start openMSX, run the first phase script (openmsx-start.sh) instead.
echo -n 1 > /proc/sys/vm/overcommit_memory
export HOME=/usr/local/home
export OPENMSX_SYSTEM_DATA=/usr/local/share/openmsx
exec /usr/local/bin/openmsx &> ~/openmsx-log.txt
