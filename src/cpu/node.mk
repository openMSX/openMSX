# $Id$

SRC_HDR:= \
	CPUInterface CPU \
	Z80 R800 \
	CPUTables \
	Dasm \
	MSXCPUInterface MSXCPU \
	MSXMultiIODevice \
	IRQHelper \
	VDPIODelay

$(eval $(PROCESS_NODE))

