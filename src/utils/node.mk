# $Id:$

include build/node-start.mk

SRC_HDR:= \
	CRC16 \
	CircularBuffer \
	Date \
	HostCPU \
	Math \
	MemoryOps \
	StringOp \
	Unicode \
	sha1

HDR_ONLY:= \
	FixedPoint \
	Subject Observer \
	ScopedAssign \
	checked_cast \
	likely \
	inline \
	noncopyable \
	shared_ptr \
	static_assert

include build/node-end.mk
