#include "EmptyPatch.hh"
#include "ranges.hh"
#include <cassert>

namespace openmsx {

void EmptyPatch::copyBlock(size_t src, byte* dst, size_t num) const
{
	auto size = block.size();
	if ((src + num) > size) {
		// past end
		if (size <= src) {
			// start past size, only fill block
			ranges::fill(std::span{dst, num}, 0);
		} else {
			auto part1 = size - src;
			auto part2 = num - part1;
			assert(dst != &block[src]);
			ranges::copy(block.subspan(src, part1), dst);
			ranges::fill(std::span{&dst[part1], part2}, 0);
		}
	} else {
		if (dst != &block[src]) {
			// ranges::copy cannot handle overlapping regions, but in
			// that case we don't need to copy at all
			ranges::copy(block.subspan(src, num), dst);
		}
	}
}

} // namespace openmsx
