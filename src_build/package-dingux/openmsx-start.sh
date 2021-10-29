#!/bin/sh
# Startup script for openMSX.
# Run this to start openMSX; running the executable directly does not work.
export OPENMSX_SYSTEM_DATA=$PWD/share
exec bin/openmsx
#exec /usr/local/bin/openmsx.dge &> /tmp/openmsx-log.txt
