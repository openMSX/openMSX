// $Id$

#include "IPSPatch.hh"
#include "File.hh"
#include "MSXException.hh"
#include <string.h>
#include <cassert>

using std::vector;

namespace openmsx {

static unsigned getStop(const IPSPatch::PatchMap::const_iterator& it)
{
	return it->first + it->second.size();
}

IPSPatch::IPSPatch(const std::string& filename,
                   std::auto_ptr<const PatchInterface> parent_)
	: parent(parent_)
{
	File ipsFile(filename);

	byte buf[5];
	ipsFile.read(buf, 5);
	if (memcmp(buf, "PATCH", 5) != 0) {
		throw FatalError("Invalid IPS file: " + filename);
	}
	ipsFile.read(buf, 3);
	while (memcmp(buf, "EOF", 3) != 0) {
		unsigned offset = 0x10000 * buf[0] + 0x100 * buf[1] + buf[2];
		ipsFile.read(buf, 2);
		unsigned length = 0x100 * buf[0] + buf[1];
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
		PatchMap::iterator b = patchMap.lower_bound(offset);
		if (b != patchMap.begin()) {
			--b;
			if (getStop(b) < offset) ++b;
		}
		PatchMap::iterator e = patchMap.upper_bound(offset + v.size());
		if (b != e) {
			// remove operlapping regions, merge adjacent regions
			--e;
			unsigned start = std::min(b->first, offset);
			unsigned stop  = std::max(offset + length, getStop(e));
			unsigned length2 = stop - start;
			++e;
			vector<byte> tmp(length2);
			for (PatchMap::iterator it = b; it != e; ++it) {
				memcpy(&tmp[it->first - start], &it->second[0],
				       it->second.size());
			}
			memcpy(&tmp[offset - start], &v[0], v.size());
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
		PatchMap::const_iterator it = --patchMap.end();
		size = std::max(parent->getSize(), getStop(it));
	}
}

void IPSPatch::copyBlock(unsigned src, byte* dst, unsigned num) const
{
	parent->copyBlock(src, dst, num);

	PatchMap::const_iterator b = patchMap.lower_bound(src);
	if (b != patchMap.begin()) --b;
	PatchMap::const_iterator e = patchMap.upper_bound(src + num - 1);
	for (PatchMap::const_iterator it = b; it != e; ++it) {
		unsigned chunkStart = it->first;
		int chunkSize = it->second.size();
		// calc chunkOffset, chunkStart
		int chunkOffset = src - chunkStart;
		if (chunkOffset < 0) {
			// start after src
			assert(-chunkOffset < (int)num); // dont start past end
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
		int overflow = chunkStart - src + chunkSize - num;
		if (overflow > 0) {
			assert(chunkSize > overflow);
			chunkSize -= overflow;
		}
		// copy
		assert(chunkOffset < (int)it->second.size());
		assert((chunkOffset + chunkSize) <= (int)it->second.size());
		assert(src <= chunkStart);
		assert((chunkStart + chunkSize) <= (src + num));
		memcpy(dst + chunkStart - src, &it->second[chunkOffset],
		       chunkSize);
	}
}

unsigned IPSPatch::getSize() const
{
	return size;
}

} // namespace openmsx
