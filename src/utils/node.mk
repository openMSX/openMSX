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
	sha1 \
	uint128 \
	DivModBySame

HDR_ONLY:= \
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
	utf8_core.hh \
	utf8_checked.hh \
	utf8_unchecked.hh \

include build/node-end.mk
