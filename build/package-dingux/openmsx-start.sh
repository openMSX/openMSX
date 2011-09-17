#!/bin/sh
# $Id$
# Startup script for openMSX.
# Run this to start openMSX; running the executable directly does not work.
echo -n 1 > /proc/sys/vm/overcommit_memory
export OPENMSX_SYSTEM_DATA=/usr/local/share/openmsx
exec /usr/local/bin/openmsx.dge
#exec /usr/local/bin/openmsx.dge &> /tmp/openmsx-log.txt
