# $Id:$

include build/node-start.mk

SRC_HDR:= \
	Base64 \
	CRC16 \
	CircularBuffer \
	Date \
	HostCPU \
	Math \
	MemoryOps \
	StringOp \
	Unicode \
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
	DivModByConst

include build/node-end.mk
