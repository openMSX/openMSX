#!/bin/sh
# Startup script for openMSX.
# Run this to start openMSX; running the executable directly does not work.
echo -n 1 > /proc/sys/vm/overcommit_memory
export HOME=/usr/local/home
export OPENMSX_SYSTEM_DATA=/usr/local/share/openmsx
exec /usr/local/bin/openmsx.dge
#exec /usr/local/bin/openmsx.dge &> ~/openmsx-log.txt
