# $Id$

include build/node-start.mk

SRC_HDR:= \
	CPU \
	Z80 R800 \
	CPUTables \
	Dasm \
	MSXCPUInterface MSXCPU \
	MSXMultiIODevice \
	IRQHelper \
	VDPIODelay

DIST:= \
	CPUCore.n1 CPUCore.n2 \
	R800Tables.nn Z80Tables.nn

include build/node-end.mk

