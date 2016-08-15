#include "DeltaBlock.hh"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace openmsx {

using std::vector;

// --- Compressed integers ---

// See https://en.wikipedia.org/wiki/LEB128 for a description of the
// 'unsigned LEB' format.
static void storeUleb(vector<uint8_t>& result, size_t value)
{
	do {
		uint8_t b = value & 0x7F;
		value >>= 7;
		if (value) b |= 0x80;
		result.push_back(b);
	} while (value);
}

static size_t loadUleb(const uint8_t*& data)
{
	size_t result = 0;
	int shift = 0;
	while (true) {
		uint8_t b = *data++;
		result |= size_t(b & 0x7F) << shift;
		if ((b & 0x80) == 0) return result;
		shift += 7;
	}
}


// --- delta (de)compression routines ---

// Calculate a 'delta' between two binary buffers of equal size.
// The result is a stream of:
//   n1 number of bytes are equal
//   n2 number of bytes are different, and here are the bytes
//   n3 number of bytes are equal
//   ...
static vector<uint8_t> calcDelta(const uint8_t* oldBuf, const uint8_t* newBuf, size_t size)
{
	// TODO possible optimization: use sentinels, scan words-at-a-time
	vector<uint8_t> result;

	auto* p = oldBuf;
	auto* q = newBuf;
	auto* end = q + size;

	// scan equal bytes (possibly zero)
	auto* q1 = q;
	while ((q != end) && (*p == *q)) { ++p; ++q; }
	auto n1 = q - q1;
	storeUleb(result, n1);

	while (q != end) {
		assert(*p != *q);

		auto* q2 = q;
	different:
		do { ++p; ++q; } while ((q != end) && (*p != *q));
		auto n2 = q - q2;

		auto* q3 = q;
		while ((q != end) && (*p == *q)) { ++p; ++q; }
		auto n3 = q - q3;
		if ((q != end) && (n3 <= 2)) goto different;

		storeUleb(result, n2);
		result.insert(result.end(), q2, q3);

		if (n3 != 0) storeUleb(result, n3);
	}

	result.shrink_to_fit();
	return result;
}

// Apply a previously calculated 'delta' to 'oldBuf' to get 'newbuf'.
static void applyDeltaInPlace(uint8_t* buf, size_t size, const uint8_t* delta)
{
	auto* end = buf + size;

	while (buf != end) {
		auto n1 = loadUleb(delta);
		buf += n1;
		if (buf == end) break;

		auto n2 = loadUleb(delta);
		memcpy(buf, delta, n2);
		buf   += n2;
		delta += n2;
	}
}


// class DeltaBlockCopy

DeltaBlockCopy::DeltaBlockCopy(const uint8_t* data, size_t size)
	: block(size)
{
	memcpy(block.data(), data, size);
}

void DeltaBlockCopy::apply(uint8_t* dst, size_t size) const
{
	memcpy(dst, block.data(), size);
}

const uint8_t* DeltaBlockCopy::getData()
{
	return block.data();
}


// class DeltaBlockDiff

DeltaBlockDiff::DeltaBlockDiff(
		const std::shared_ptr<DeltaBlockCopy>& prev_,
		const uint8_t* data, size_t size)
	: prev(prev_)
	, delta(calcDelta(prev->getData(), data, size))
{
}

void DeltaBlockDiff::apply(uint8_t* dst, size_t size) const
{
	prev->apply(dst, size);
	applyDeltaInPlace(dst, size, delta.data());
}

size_t DeltaBlockDiff::getDeltaSize() const
{
	return delta.size();
}


// class LastDeltaBlocks

std::shared_ptr<DeltaBlock> LastDeltaBlocks::createNew(
		const void* id, const uint8_t* data, size_t size)
{
	auto it = std::lower_bound(begin(infos), end(infos), id,
		[](const Info& info, const void* id2) {
			return info.id < id2; });
	if ((it == end(infos)) || (it->id != id)) {
		// no previous info yet
		it = infos.emplace(it, id, size);
	}
	assert(it->size == size);

	auto ref = it->ref.lock();
	if (it->accSize >= size || !ref) {
		// Heuristic: create a new block when too many small
		// differences have accumulated.
		auto b = std::make_shared<DeltaBlockCopy>(data, size);
		it->ref = b;
		it->accSize = 0;
		return b;
	} else {
		// Create diff based on earlier reference block.
		// Reference remains unchanged.
		auto b = std::make_shared<DeltaBlockDiff>(ref, data, size);
		it->accSize += b->getDeltaSize();
		return b;
	}
}

void LastDeltaBlocks::clear()
{
	infos.clear();
}

} // namespace openmsx
