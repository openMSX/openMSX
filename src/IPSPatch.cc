#include "IPSPatch.hh"

#include "File.hh"
#include "Filename.hh"
#include "MSXException.hh"

#include "ranges.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>

namespace openmsx {

std::vector<IPSPatch::Chunk> IPSPatch::parseChunks() const
{
	std::vector<Chunk> result;

	File ipsFile(filename);

	std::array<uint8_t, 5> header;
	ipsFile.read(header);
	if (!std::ranges::equal(header, std::string_view("PATCH"))) {
		throw MSXException("Invalid IPS file: ", filename.getOriginal());
	}
	std::array<uint8_t, 3> offsetBuf;
	ipsFile.read(offsetBuf);
	while (!std::ranges::equal(offsetBuf, std::string_view("EOF"))) {
		size_t offset = 0x10000 * offsetBuf[0] + 0x100 * offsetBuf[1] + offsetBuf[2];
		std::array<uint8_t, 2> lenBuf;
		ipsFile.read(lenBuf);
		size_t length = 0x100 * lenBuf[0] + lenBuf[1];
		std::vector<uint8_t> v;
		if (length == 0) {
			// RLE encoded
			std::array<uint8_t, 3> rleBuf;
			ipsFile.read(rleBuf);
			length = 0x100 * rleBuf[0] + rleBuf[1];
			v.resize(length, rleBuf[2]);
		} else {
			// patch bytes
			v.resize(length);
			ipsFile.read(v);
		}
		// find overlapping or adjacent patch regions
		auto b = std::ranges::lower_bound(result, offset, {}, &Chunk::startAddress);
		if (b != begin(result)) {
			--b;
			if (b->stopAddress() < offset) ++b;
		}
		if (auto e = std::ranges::upper_bound(result, offset + v.size(), {}, &Chunk::startAddress);
		    b != e) {
			// remove overlapping regions, merge adjacent regions
			--e;
			auto start = std::min(b->startAddress, offset);
			auto stop  = std::max(offset + length, e->stopAddress());
			auto length2 = stop - start;
			++e;
			std::vector<uint8_t> tmp(length2);
			for (auto it : xrange(b, e)) {
				copy_to_range(*it, subspan(tmp, it->startAddress - start));
			}
			copy_to_range(v, subspan(tmp, offset - start));
			*b = Chunk{start, std::move(tmp)};
			result.erase(b + 1, e);
		} else {
			// add new region
			result.emplace(b, offset, std::move(v));
		}

		ipsFile.read(offsetBuf);
	}
	return result;
}

size_t IPSPatch::calcSize() const
{
	return chunks.empty()
		? parent->getSize()
		: std::max(parent->getSize(), chunks.back().stopAddress());
}

IPSPatch::IPSPatch(Filename filename_,
                   std::unique_ptr<const PatchInterface> parent_)
	: filename(std::move(filename_))
	, parent(std::move(parent_))
	, chunks(parseChunks())
	, size(calcSize())
{
}

void IPSPatch::copyBlock(size_t src, std::span<uint8_t> dst) const
{
	parent->copyBlock(src, dst);

	auto b = std::ranges::lower_bound(chunks, src, {}, &Chunk::startAddress);
	if (b != begin(chunks)) --b;
	auto srcEnd = src + dst.size();
	auto e = std::ranges::upper_bound(chunks, srcEnd - 1, {}, &Chunk::startAddress);
	for (auto it : xrange(b, e)) {
		auto chunkStart = it->startAddress;
		auto chunkSize = int(it->size());
		// calc chunkOffset, chunkStart
		auto chunkOffset = int(src - chunkStart);
		if (chunkOffset < 0) {
			// start after src
			assert(-chunkOffset < int(dst.size())); // dont start past end
			chunkOffset = 0;
		} else if (chunkOffset >= chunkSize) {
			// chunk completely before src, skip
			continue;
		} else {
			// start before src
			chunkSize  -= chunkOffset;
			chunkStart += chunkOffset;
		}
		// calc chunkSize
		assert(src <= chunkStart);
		if (auto overflow = int(chunkStart - src + chunkSize - dst.size());
		    overflow > 0) {
			assert(chunkSize > overflow);
			chunkSize -= overflow;
		}
		// copy
		assert(chunkOffset < int(it->size()));
		assert((chunkOffset + chunkSize) <= int(it->size()));
		assert(src <= chunkStart);
		assert((chunkStart + chunkSize) <= srcEnd);
		copy_to_range(subspan(*it, chunkOffset, size_t(chunkSize)),
		              subspan(dst, chunkStart - src));
	}
}

std::vector<Filename> IPSPatch::getFilenames() const
{
	auto result = parent->getFilenames();
	result.push_back(filename);
	return result;
}

} // namespace openmsx
