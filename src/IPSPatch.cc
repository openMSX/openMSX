#include "IPSPatch.hh"
#include "File.hh"
#include "Filename.hh"
#include "MSXException.hh"
#include <algorithm>
#include <cstring>
#include <cassert>

using std::vector;

namespace openmsx {

static size_t getStop(const IPSPatch::PatchMap::const_iterator& it)
{
	return it->first + it->second.size();
}

IPSPatch::IPSPatch(const Filename& filename_,
                   std::unique_ptr<const PatchInterface> parent_)
	: filename(filename_)
	, parent(std::move(parent_))
{
	File ipsFile(filename);

	byte buf[5];
	ipsFile.read(buf, 5);
	if (memcmp(buf, "PATCH", 5) != 0) {
		throw MSXException("Invalid IPS file: " + filename.getOriginal());
	}
	ipsFile.read(buf, 3);
	while (memcmp(buf, "EOF", 3) != 0) {
		size_t offset = 0x10000 * buf[0] + 0x100 * buf[1] + buf[2];
		ipsFile.read(buf, 2);
		size_t length = 0x100 * buf[0] + buf[1];
		vector<byte> v;
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
		auto b = patchMap.lower_bound(offset);
		if (b != patchMap.begin()) {
			--b;
			if (getStop(b) < offset) ++b;
		}
		auto e = patchMap.upper_bound(offset + v.size());
		if (b != e) {
			// remove operlapping regions, merge adjacent regions
			--e;
			auto start = std::min(b->first, offset);
			auto stop  = std::max(offset + length, getStop(e));
			auto length2 = stop - start;
			++e;
			vector<byte> tmp(length2);
			for (auto it = b; it != e; ++it) {
				memcpy(&tmp[it->first - start], &it->second[0],
				       it->second.size());
			}
			memcpy(&tmp[offset - start], v.data(), v.size());
			patchMap.erase(b, e);
			patchMap[start] = tmp;
		} else {
			// add new region
			patchMap[offset] = v;
		}

		ipsFile.read(buf, 3);
	}
	if (patchMap.empty()) {
		size = parent->getSize();
	} else {
		auto it = --patchMap.end();
		size = std::max(parent->getSize(), getStop(it));
	}
}

void IPSPatch::copyBlock(size_t src, byte* dst, size_t num) const
{
	parent->copyBlock(src, dst, num);

	auto b = patchMap.lower_bound(src);
	if (b != patchMap.begin()) --b;
	auto e = patchMap.upper_bound(src + num - 1);
	for (auto it = b; it != e; ++it) {
		auto chunkStart = it->first;
		int chunkSize = int(it->second.size());
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
		// calc chuncksize
		assert(src <= chunkStart);
		int overflow = int(chunkStart - src + chunkSize - num);
		if (overflow > 0) {
			assert(chunkSize > overflow);
			chunkSize -= overflow;
		}
		// copy
		assert(chunkOffset < int(it->second.size()));
		assert((chunkOffset + chunkSize) <= int(it->second.size()));
		assert(src <= chunkStart);
		assert((chunkStart + chunkSize) <= (src + num));
		memcpy(dst + chunkStart - src, &it->second[chunkOffset],
		       chunkSize);
	}
}

size_t IPSPatch::getSize() const
{
	return size;
}

std::vector<Filename> IPSPatch::getFilenames() const
{
	auto result = parent->getFilenames();
	result.push_back(filename);
	return result;
}

} // namespace openmsx
