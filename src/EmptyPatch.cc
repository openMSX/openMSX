// $Id$

#include "EmptyPatch.hh"
#include <string.h>
#include <cassert>

namespace openmsx {

EmptyPatch::EmptyPatch(const byte* block_, unsigned size_)
	: block(block_), size(size_)
{
}

void EmptyPatch::copyBlock(unsigned src, byte* dst, unsigned num) const
{
	assert((src + num) <= size);
	if (dst != block) {
		// memcpy cannot handle overlapping regions, but in that case
		// we don't need to copy at all
		memcpy(dst, &block[src], num);
	}
}

unsigned EmptyPatch::getSize() const
{
	return size;
}

} // namespace openmsx

