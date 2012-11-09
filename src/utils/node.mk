# $Id$

include build/node-start.mk

SRC_HDR:= \
	Base64 HexDump \
	CRC16 \
	CircularBuffer \
	SerializeBuffer \
	Date \
	HostCPU \
	Math \
	MemoryOps \
	StringMap \
	StringOp \
	sha1 \
	uint128 \
	DivModBySame \
	AltSpaceSuppressor sdlwin32 utf8_checked win32-dirent win32-arggen \
	snappy \
	string_ref \
	rapidsax \

HDR_ONLY:= \
	FixedPoint \
	MemBuffer \
	Subject Observer \
	ScopedAssign \
	checked_cast \
	endian \
	alignof \
	likely \
	inline \
	noncopyable \
	unreachable \
	tuple \
	TypeInfo \
	type_traits \
	DivModByConst \
	utf8_core \
	utf8_unchecked \
	aligned cstdiop cstdlibp direntp statp stringsp unistdp vla win32-arggen \
	countof

include build/node-end.mk
