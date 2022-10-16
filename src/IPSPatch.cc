#include "IPSPatch.hh"
#include "File.hh"
#include "Filename.hh"
#include "MSXException.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

std::vector<IPSPatch::Chunk> IPSPatch::parseChunks() const
{
	std::vector<Chunk> result;

	File ipsFile(filename);

	byte buf[5];
	ipsFile.read(buf, 5);
	if (memcmp(buf, "PATCH", 5) != 0) {
		throw MSXException("Invalid IPS file: ", filename.getOriginal());
	}
	ipsFile.read(buf, 3);
	while (memcmp(buf, "EOF", 3) != 0) {
		size_t offset = 0x10000 * buf[0] + 0x100 * buf[1] + buf[2];
		ipsFile.read(buf, 2);
		size_t length = 0x100 * buf[0] + buf[1];
		std::vector<byte> v;
		if (length == 0) {
			// RLE encoded
			ipsFile.read(buf, 3);
			length = 0x100 * buf[0] + buf[1];
			v.resize(length, buf[2]);
		} else {
			// patch bytes
			v.resize(length);
			ipsFile.read(&v.front(), length);
		}
		// find overlapping or adjacent patch regions
		auto b = ranges::lower_bound(result, offset, {}, &Chunk::startAddress);
		if (b != begin(result)) {
			--b;
			if (b->stopAddress() < offset) ++b;
		}
		auto e = ranges::upper_bound(result, offset + v.size(), {}, &Chunk::startAddress);
		if (b != e) {
			// remove overlapping regions, merge adjacent regions
			--e;
			auto start = std::min(b->startAddress, offset);
			auto stop  = std::max(offset + length, e->stopAddress());
			auto length2 = stop - start;
			++e;
			std::vector<byte> tmp(length2);
			for (auto it : xrange(b, e)) {
				ranges::copy(*it, &tmp[it->startAddress - start]);
			}
			ranges::copy(v, &tmp[offset - start]);
			*b = Chunk{start, std::move(tmp)};
			result.erase(b + 1, e);
		} else {
			// add new region
			result.emplace(b, Chunk{offset, std::move(v)});
		}

		ipsFile.read(buf, 3);
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

void IPSPatch::copyBlock(size_t src, byte* dst, size_t num) const
{
	parent->copyBlock(src, dst, num);

	auto b = ranges::lower_bound(chunks, src, {}, &Chunk::startAddress);
	if (b != begin(chunks)) --b;
	auto e = ranges::upper_bound(chunks, src + num - 1, {}, &Chunk::startAddress);
	for (auto it : xrange(b, e)) {
		auto chunkStart = it->startAddress;
		int chunkSize = int(it->size());
		// calc chunkOffset, chunkStart
		int chunkOffset = int(src - chunkStart);
		if (chunkOffset < 0) {
			// start after src
			assert(-chunkOffset < int(num)); // dont start past end
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
		int overflow = int(chunkStart - src + chunkSize - num);
		if (overflow > 0) {
			assert(chunkSize > overflow);
			chunkSize -= overflow;
		}
		// copy
		assert(chunkOffset < int(it->size()));
		assert((chunkOffset + chunkSize) <= int(it->size()));
		assert(src <= chunkStart);
		assert((chunkStart + chunkSize) <= (src + num));
		ranges::copy(std::span{it->data() + chunkOffset, size_t(chunkSize)},
		             dst + chunkStart - src);
	}
}

std::vector<Filename> IPSPatch::getFilenames() const
{
	auto result = parent->getFilenames();
	result.push_back(filename);
	return result;
}

} // namespace openmsx
