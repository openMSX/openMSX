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
	if ((src + num) > size) {
		// past end
		if (size <= src) {
			// start past size, only fill block
			memset(dst, 0, num);
		} else {
			unsigned part1 = size - src;
			unsigned part2 = num - part1;
			assert(dst != (block + src));
			memcpy(dst, &block[src], part1);
			memset(dst + part1, 0, part2);
		}
	} else {
		if (dst != (block + src)) {
			// memcpy cannot handle overlapping regions, but in
			// that case we don't need to copy at all
			memcpy(dst, &block[src], num);
		}
	}
}

unsigned EmptyPatch::getSize() const
{
	return size;
}

} // namespace openmsx

