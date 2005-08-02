# $Id$

include build/node-start.mk

SRC_HDR:= \
	Debugger \
	DasmTables \
	SimpleDebuggable

HDR_ONLY:= \
	Debuggable

include build/node-end.mk

