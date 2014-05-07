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
	AltSpaceSuppressor win32-arggen win32-dirent win32-windowhandle \
	utf8_checked \
	snappy \
	string_ref \
	rapidsax \

HDR_ONLY:= \
	FixedPoint \
	KeyRange \
	AlignedBuffer \
	MemBuffer \
	Subject Observer \
	ScopedAssign \
	checked_cast \
	circular_buffer \
	endian \
	alignof \
	array_ref \
	likely \
	inline \
	memory \
	noncopyable \
	unreachable \
	stl \
	type_traits \
	DivModByConst \
	utf8_core \
	utf8_unchecked \
	aligned cstdiop direntp statp stringsp unistdp vla win32-arggen \
	countof \
	xrange \
	xxhash

include build/node-end.mk
