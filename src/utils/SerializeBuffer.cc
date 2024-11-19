#include "SerializeBuffer.hh"
#include <cstdlib>
#include <utility>

namespace openmsx {

// class OutputBuffer

OutputBuffer::OutputBuffer()
	: buf(lastSize)
	, end(buf.data())
{
	// We've allocated a buffer with an estimated initial size. This
	// estimate is based on the largest intermediate size of the previously
	// required buffers.
	// For correctness this initial size doesn't matter (we could even not
	// allocate any initial buffer at all). But it can make a difference in
	// performance. If later we discover the buffer is too small, we have
	// to reallocate (and thus make a copy). In profiling this reallocation
	// step was noticeable.

	// Slowly drop the estimated required size. This makes sure that when
	// we've overestimated the size once, we don't forever keep this too
	// high value. For performance an overestimation is less bad than an
	// underestimation.
	lastSize -= lastSize >> 7;
}

#ifdef __GNUC__
template<size_t LEN> void OutputBuffer::insertN(const void* __restrict data)
{
	uint8_t* newEnd = end + LEN;
	if (newEnd <= buf.end()) [[likely]] {
		memcpy(end, data, LEN);
		end = newEnd;
	} else {
		insertGrow(data, LEN);
	}
}
// Force template instantiation
template void OutputBuffer::insertN<1>(const void* __restrict data);
template void OutputBuffer::insertN<2>(const void* __restrict data);
template void OutputBuffer::insertN<4>(const void* __restrict data);
template void OutputBuffer::insertN<8>(const void* __restrict data);
#endif

void OutputBuffer::insertN(const void* __restrict data, size_t len)
{
	uint8_t* newEnd = end + len;
	if (newEnd <= buf.end()) [[likely]] {
		memcpy(end, data, len);
		end = newEnd;
	} else {
		insertGrow(data, len);
	}
}

MemBuffer<uint8_t> OutputBuffer::release() &&
{
	// Deallocate unused buffer space.
	size_t size = end - buf.data();
	buf.resize(size);

	end = nullptr;
	return std::move(buf);
}

void OutputBuffer::grow(size_t len)
{
	size_t oldSize = end - buf.data();
	size_t newSize = std::max(oldSize + len, oldSize + oldSize / 2);
	buf.resize(newSize);
	end    = buf.data() + oldSize;
}

uint8_t* OutputBuffer::allocateGrow(size_t len)
{
	grow(len);
	auto* result = end;
	end += len;
	return result;
}

void OutputBuffer::insertGrow(const void* __restrict data, size_t len)
{
	uint8_t* pos = allocateGrow(len);
	memcpy(pos, data, len);
}

} // namespace openmsx
