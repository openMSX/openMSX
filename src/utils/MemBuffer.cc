// $Id$

#include "MemBuffer.hh"
#include <cstdlib>

namespace openmsx {

MemBuffer::MemBuffer()
	: dat(NULL)
	, sz(0)
{
}

MemBuffer::MemBuffer(byte* data, unsigned size)
	: dat(data)
	, sz(size)
{
}

MemBuffer::~MemBuffer()
{
	free(dat);
}

void MemBuffer::resize(unsigned size)
{
	dat = static_cast<byte*>(::realloc(dat, size));
	sz = size;
}

} // namespace openmsx
