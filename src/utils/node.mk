# $Id$

include build/node-start.mk

SRC_HDR:= \
	Base64 HexDump \
	CRC16 \
	CircularBuffer \
	SerializeBuffer \
	MemBuffer \
	Date \
	HostCPU \
	Math \
	MemoryOps \
	StringOp \
	sha1 \
	uint128 \
	DivModBySame \
	AltSpaceSuppressor sdlwin32 utf8_checked win32-dirent win32-arggen \
	lzo

HDR_ONLY:= \
	my_auto_ptr \
	FixedPoint \
	Subject Observer \
	ScopedAssign \
	checked_cast \
	likely \
	inline \
	ref \
	noncopyable \
	shared_ptr \
	static_assert \
	unreachable \
	tuple \
	TypeInfo \
	type_traits \
	DivModByConst \
	utf8_core \
	utf8_unchecked \
	aligned cstdiop cstdlibp direntp statp stringsp unistdp vla win32-arggen

include build/node-end.mk
