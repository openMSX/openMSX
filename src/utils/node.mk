include build/node-start.mk

SRC_HDR:= \
	Base64 HexDump \
	CRC16 \
	CircularBuffer \
	SerializeBuffer \
	Date \
	Math \
	MemoryOps \
	StringMap \
	StringOp \
	sha1 \
	tiger \
	TigerTree \
	uint128 \
	DivModBySame \
	AltSpaceSuppressor sdlwin32 utf8_checked win32-dirent win32-arggen \
	snappy \
	string_ref \
	rapidsax \

HDR_ONLY:= \
	FixedPoint \
	KeyRange \
	MemBuffer \
	Subject Observer \
	ScopedAssign \
	checked_cast \
	endian \
	alignof \
	array_ref \
	likely \
	inline \
	memory \
	noncopyable \
	unreachable \
	TypeInfo \
	type_traits \
	DivModByConst \
	utf8_core \
	utf8_unchecked \
	aligned cstdiop cstdlibp direntp statp stringsp unistdp vla win32-arggen \
	countof \
	xrange

include build/node-end.mk
