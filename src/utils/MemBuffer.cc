// $Id$

#include "MemBuffer.hh"
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

void OutputBuffer::insertGrow(const void* __restrict data, unsigned len)
{
	unsigned oldSize = end - begin;
	unsigned newSize = std::max(oldSize + len, oldSize + oldSize / 2);
	begin = reinterpret_cast<char*>(realloc(begin, newSize));
	end = begin + oldSize + len;
	finish = begin + newSize;
	memcpy(begin + oldSize, data, len);
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
