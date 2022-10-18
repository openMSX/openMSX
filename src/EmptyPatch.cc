#include "EmptyPatch.hh"
#include "ranges.hh"
#include <cassert>

namespace openmsx {

void EmptyPatch::copyBlock(size_t src, std::span<uint8_t> dst) const
{
	auto size = block.size();
	if ((src + dst.size()) > size) {
		// past end
		if (size <= src) {
			// start past size, only fill block
			ranges::fill(dst, 0);
		} else {
			auto part1 = size - src;
			auto part2 = dst.size() - part1;
			assert(dst.data() != &block[src]);
			ranges::copy(block.subspan(src, part1), dst);
			ranges::fill(dst.subspan(part1, part2), 0);
		}
	} else {
		if (dst.data() != &block[src]) {
			// ranges::copy cannot handle overlapping regions, but in
			// that case we don't need to copy at all
			ranges::copy(block.subspan(src, dst.size()), dst);
		}
	}
}

} // namespace openmsx
