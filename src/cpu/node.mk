# $Id$

include build/node-start.mk

SRC_HDR:= \
	CPU CPUCore Dasm \
	Z80 R800 \
	MSXCPUInterface MSXCPU \
	MSXMultiIODevice \
	IRQHelper \
	VDPIODelay \

include build/node-end.mk

