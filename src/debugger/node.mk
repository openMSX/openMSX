# $Id$

include build/node-start.mk

SRC_HDR:= \
	Debugger \
	DasmTables \
	SimpleDebuggable \
	Probe

HDR_ONLY:= \
	Debuggable

include build/node-end.mk

