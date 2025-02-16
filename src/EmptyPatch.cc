#include "EmptyPatch.hh"

#include "ranges.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

void EmptyPatch::copyBlock(size_t src, std::span<uint8_t> dst) const
{
	auto size = block.size();
	if ((src + dst.size()) > size) {
		// past end
		if (size <= src) {
			// start past size, only fill block
			std::ranges::fill(dst, 0);
		} else {
			auto part1 = size - src;
			auto part2 = dst.size() - part1;
			assert(dst.data() != &block[src]);
			copy_to_range(block.subspan(src, part1), dst);
			std::ranges::fill(dst.subspan(part1, part2), 0);
		}
	} else {
		if (dst.data() != &block[src]) {
			// copy_to_range cannot handle overlapping regions, but in
			// that case we don't need to copy at all
			copy_to_range(block.subspan(src, dst.size()), dst);
		}
	}
}

} // namespace openmsx
