// $Id$

#include "MemBuffer.hh"
#include "likely.hh"
#include <algorithm>
#include <cstdlib>

namespace openmsx {

OutputBuffer::OutputBuffer()
	: begin(NULL), end(NULL), finish(NULL)
{
}

OutputBuffer::~OutputBuffer()
{
	free(begin);
}

#ifdef __GNUC__
template<unsigned LEN> void OutputBuffer::insertN(const void* __restrict data) __restrict
{
	byte* newEnd = end + LEN;
	if (likely(newEnd <= finish)) {
		memcpy(end, data, LEN);
		end = newEnd;
	} else {
		insertGrow(data, LEN);
	}
}
// Force template instantiation
template void OutputBuffer::insertN<1>(const void* __restrict data) __restrict;
template void OutputBuffer::insertN<2>(const void* __restrict data) __restrict;
template void OutputBuffer::insertN<4>(const void* __restrict data) __restrict;
template void OutputBuffer::insertN<8>(const void* __restrict data) __restrict;
#endif

void OutputBuffer::insertN(const void* __restrict data, unsigned len) __restrict
{
	byte* newEnd = end + len;
	if (likely(newEnd <= finish)) {
		memcpy(end, data, len);
		end = newEnd;
	} else {
		insertGrow(data, len);
	}
}

byte* OutputBuffer::allocate(unsigned len)
{
	byte* newEnd = end + len;
	if (newEnd <= finish) {
		byte* result = end;
		end = newEnd;
		return result;
	} else {
		return allocateGrow(len);
	}
}

void OutputBuffer::deallocate(byte* pos)
{
	assert(begin <= pos);
	assert(pos <= end);
	end = pos;
}

void OutputBuffer::insertGrow(const void* __restrict data, unsigned len) __restrict
{
	byte* pos = allocateGrow(len);
	memcpy(pos, data, len);
}

byte* OutputBuffer::allocateGrow(unsigned len)
{
	unsigned oldSize = end - begin;
	unsigned newSize = std::max(oldSize + len, oldSize + oldSize / 2);
	begin = static_cast<byte*>(realloc(begin, newSize));
	end = begin + oldSize + len;
	finish = begin + newSize;
	return begin + oldSize;
}

MemBuffer::MemBuffer()
	: data(NULL)
	, len(0)
{
}

MemBuffer::MemBuffer(OutputBuffer& buffer)
{
	data = buffer.begin;
	len = buffer.end - buffer.begin;
	buffer.begin = buffer.end = buffer.finish = NULL;
}

MemBuffer::~MemBuffer()
{
	free(data);
}

void MemBuffer::realloc(unsigned newSize)
{
	data = static_cast<byte*>(::realloc(data, newSize));
	len = newSize;
}


InputBuffer::InputBuffer(const byte* data, unsigned size)
	: buf(data)
#ifndef NDEBUG
	, finish(buf + size)
#endif
{
	(void)size;
}

} // namespace openmsx
