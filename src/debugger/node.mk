# $Id$

SRC_HDR:= \
	DebugView \
	MemoryView \
	DumpView \
	DisAsmView \
	DebugConsole \
	ViewControl \
	DasmTables \
	Debugger

HDR_ONLY:= \
	Debuggable

$(eval $(PROCESS_NODE))

