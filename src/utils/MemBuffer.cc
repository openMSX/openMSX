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
template<unsigned LEN> void OutputBuffer::insertN(const void* __restrict data)
{
	char* newEnd = end + LEN;
	if (likely(newEnd <= finish)) {
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

void OutputBuffer::insertN(const void* __restrict data, unsigned len)
{
	char* newEnd = end + len;
	if (likely(newEnd <= finish)) {
		memcpy(end, data, len);
		end = newEnd;
	} else {
		insertGrow(data, len);
	}
}

char* OutputBuffer::allocate(unsigned len)
{
	char* newEnd = end + len;
	if (newEnd <= finish) {
		char* result = end;
		end = newEnd;
		return result;
	} else {
		return allocateGrow(len);
	}
}

void OutputBuffer::deallocate(char* pos)
{
	assert(begin <= pos);
	assert(pos <= end);
	end = pos;
}

void OutputBuffer::insertGrow(const void* __restrict data, unsigned len)
{
	char* pos = allocateGrow(len);
	memcpy(pos, data, len);
}

char* OutputBuffer::allocateGrow(unsigned len)
{
	unsigned oldSize = end - begin;
	unsigned newSize = std::max(oldSize + len, oldSize + oldSize / 2);
	begin = reinterpret_cast<char*>(realloc(begin, newSize));
	end = begin + oldSize + len;
	finish = begin + newSize;
	return begin + oldSize;
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


InputBuffer::InputBuffer(const char* data, unsigned size)
	: buf(data)
#ifndef NDEBUG
	, finish(buf + size)
#endif
{
	(void)size;
}

} // namespace openmsx
