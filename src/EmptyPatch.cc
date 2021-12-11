#include "EmptyPatch.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

EmptyPatch::EmptyPatch(const byte* block_, size_t size_)
	: block(block_), size(size_)
{
}

void EmptyPatch::copyBlock(size_t src, byte* dst, size_t num) const
{
	if ((src + num) > size) {
		// past end
		if (size <= src) {
			// start past size, only fill block
			memset(dst, 0, num);
		} else {
			auto part1 = size - src;
			auto part2 = num - part1;
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

size_t EmptyPatch::getSize() const
{
	return size;
}

std::vector<Filename> EmptyPatch::getFilenames() const
{
	return {};
}

} // namespace openmsx
