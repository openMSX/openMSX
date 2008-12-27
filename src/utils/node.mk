# $Id:$

include build/node-start.mk

SRC_HDR:= \
	Base64 HexDump \
	CRC16 \
	CircularBuffer \
	Date \
	HostCPU \
	Math \
	MemoryOps \
	StringOp \
	StringPool \
	sha1 \
	uint128 \
	DivModBySame

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
	tuple \
	TypeInfo \
	type_traits \
	DivModByConst \
	utf8_core \
	utf8_checked \
	utf8_unchecked \

include build/node-end.mk
