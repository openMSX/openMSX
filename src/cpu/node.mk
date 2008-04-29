# $Id$

include build/node-start.mk

SRC_HDR:= \
	CPU CPUCore CPUClock Dasm \
	BreakPointBase BreakPoint WatchPoint \
	MSXCPUInterface MSXCPU \
	MSXMultiDevice MSXMultiIODevice MSXMultiMemDevice \
	MSXWatchIODevice \
	VDPIODelay \
	IRQHelper

HDR_ONLY:= \
	Z80 R800 \
	CacheLine

include build/node-end.mk

