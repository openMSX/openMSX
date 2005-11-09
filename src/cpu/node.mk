# $Id$

include build/node-start.mk

SRC_HDR:= \
	CPU CPUCore Dasm \
	Z80 R800 \
	BreakPoint \
	MSXCPUInterface MSXCPU \
	MSXMultiIODevice MSXMultiMemDevice \
	VDPIODelay

HDR_ONLY:= \
	IRQHelper

include build/node-end.mk

