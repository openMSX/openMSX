#include "DeltaBlock.hh"
#include "snappy.hh"
#include "likely.hh"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <tuple>
#if STATISTICS
#include <iostream>
#endif

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


// --- Optimized scan_{mis,}match functions ---

// This is much like the function std::mismatch(). You pass in two buffers,
// the corresponding elements of both buffers are compared and the first
// position where the elements no longer match is returned.
//
// Here we have buffers of bytes and the criteria for 'match' are 'the bytes
// are equal' or 'the bytes are not equal'. In other words, we pass two buffers
// and as a result we get the (first) position where the bytes are different or
// equal.
//
// Compared to the std::mismatch() this implementation is faster because:
//  - We make use of sentinels (this requires to temporary change the content
//    of the buffer).
//  - We compare words-at-a-time instead of element-at-a-time and in
//    doing so possibly read a bit beyond the end of the buffer (more on this
//    below).
// The generic std implementation is usually not allowed to exploit these
// properties. (And the libstdc++ version indeed doesn't).


// Scan 2 buffers for the first difference. We require there is a difference
// before the end of the buffer(s) is reached.
static std::pair<const uint8_t*, const uint8_t*> scan_mismatch_unguarded_simple(
	const uint8_t* p, const uint8_t* q)
{
	while (*p == *q) { ++p; ++q; }
	return {p, q};
}

// Same as the function above but tries to compare both buffers word-at-a-time
// instead of byte-at-a-time. This is only possible when both buffers have the
// same alignment (this is almost always the case). In case they have different
// alignment this function falls back to the slower version above.
//
// In case the buffers don't end on a word-boundary this routine might read some
// bytes beyond the end of the buffer, but:
//  - Those bytes don't influence the result (because we require there's a
//    difference before the end of the buffer).
//  - Because we only read aligned words an we never read more than one word
//    past the end we never read from the next memory page, so we won't trigger
//    any memory protection mechanisms.
static std::pair<const uint8_t*, const uint8_t*> scan_mismatch_unguarded(
	const uint8_t* p, const uint8_t* q)
{
	if (unlikely((reinterpret_cast<uintptr_t>(p) & 7) !=
	             (reinterpret_cast<uintptr_t>(q) & 7))) {
		return scan_mismatch_unguarded_simple(p, q);
	}

	if (reinterpret_cast<uintptr_t>(p) & 1) {
		if (*reinterpret_cast<const uint8_t*>(p) !=
		    *reinterpret_cast<const uint8_t*>(q)) {
			goto end1;
		}
		p += 1; q += 1;
	}
	if (reinterpret_cast<uintptr_t>(p) & 2) {
		if (*reinterpret_cast<const uint16_t*>(p) !=
		    *reinterpret_cast<const uint16_t*>(q)) {
			goto end2;
		}
		p += 2; q += 2;
	}
	if (reinterpret_cast<uintptr_t>(p) & 4) {
		if (*reinterpret_cast<const uint32_t*>(p) !=
		    *reinterpret_cast<const uint32_t*>(q)) {
			goto end4;
		}
		p += 4; q += 4;
	}

	while (*reinterpret_cast<const uint64_t*>(p) ==
	       *reinterpret_cast<const uint64_t*>(q)) {
		p += 8; q += 8;
	}

	if (*reinterpret_cast<const uint32_t*>(p) ==
	    *reinterpret_cast<const uint32_t*>(q)) {
		p += 4; q += 4;
	}
end4:	if (*reinterpret_cast<const uint16_t*>(p) ==
	    *reinterpret_cast<const uint16_t*>(q)) {
		p += 2; q += 2;
	}
end2:	if (*reinterpret_cast<const uint8_t*>(p) ==
	    *reinterpret_cast<const uint8_t*>(q)) {
		p += 1; q += 1;
	}

end1:	return {p, q};

}

// Similar as above, but scan for equal bytes. We require there is a pair of
// equal bytes before the end of the buffer(s) is reached.
static std::pair<const uint8_t*, const uint8_t*> scan_match_unguarded(
	const uint8_t* p, const uint8_t* q)
{
	while (*p != *q) { ++p; ++q; }
	return {p, q};
}

// As above, but does allow two fully equal buffers. In that case the pointers
// immediately past the end of the buffers are returned. Both buffers must have
// the same size.
//
// Internally this function will place a sentinel in the buffers (and restore
// it afterwards). So even though this function takes 'const pointers' it does
// temporarily write to the buffer (and it will crash when you pass pointers to
// read-only memory).
static std::pair<const uint8_t*, const uint8_t*> scan_mismatch(
	const uint8_t* p, const uint8_t* p_end, const uint8_t* q, const uint8_t* q_end)
{
	assert((p_end - p) == (q_end - q));

	// Code below is functionally equivalent to:
	//   while ((q != q_end) && (*p == *q)) { ++p; ++q; }
	//   return {p, q};

	if (p == p_end) return {p, q};

	auto* p_last = const_cast<uint8_t*>(p_end - 1);
	auto save = *p_last;
	*p_last = ~q_end[-1]; // make p_end[-1] != q_end[-1]

	std::tie(p, q) = scan_mismatch_unguarded(p, q);

	*p_last = save;
	if ((p == p_last) && (*p == *q)) { ++p; ++q; }
	return {p, q};
}

// As above, but searches for equal corresponding bytes.
static std::pair<const uint8_t*, const uint8_t*> scan_match(
	const uint8_t* p, const uint8_t* p_end, const uint8_t* q, const uint8_t* q_end)
{
	assert((p_end - p) == (q_end - q));

	// Code below is functionally equivalent to:
	//   while ((q != q_end) && (*p != *q)) { ++p; ++q; }
	//   return {p, q};

	if (p == p_end) return {p, q};

	auto* p_last = const_cast<uint8_t*>(p_end - 1);
	auto save = *p_last;
	*p_last = q_end[-1]; // make p_end[-1] == q_end[-1]

	std::tie(p, q) = scan_match_unguarded(p, q);

	*p_last = save;
	if ((p == p_last) && (*p != *q)) { ++p; ++q; }
	return {p, q};
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
	vector<uint8_t> result;

	auto* p = oldBuf;
	auto* q = newBuf;
	auto* p_end = p + size;
	auto* q_end = q + size;

	// scan equal bytes (possibly zero)
	auto* q1 = q;
	std::tie(p, q) = scan_mismatch(p, p_end, q, q_end);
	auto n1 = q - q1;
	storeUleb(result, n1);

	while (q != q_end) {
		assert(*p != *q);

		auto* q2 = q;
	different:
		std::tie(p, q) = scan_match(p + 1, p_end, q + 1, q_end);
		auto n2 = q - q2;

		auto* q3 = q;
		std::tie(p, q) = scan_mismatch(p, p_end, q, q_end);
		auto n3 = q - q3;
		if ((q != q_end) && (n3 <= 2)) goto different;

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

#if STATISTICS

// class DeltaBlock

size_t DeltaBlock::globalAllocSize = 0;

DeltaBlock::~DeltaBlock()
{
	globalAllocSize -= allocSize;
	std::cout << "stat: ~DeltaBlock " << globalAllocSize
	          << " (-" << allocSize << ')' << std::endl;
}

#endif

// class DeltaBlockCopy

DeltaBlockCopy::DeltaBlockCopy(const uint8_t* data, size_t size)
	: block(size)
	, compressedSize(0)
{
#ifdef DEBUG
	sha1 = SHA1::calc(data, size);
#endif
	memcpy(block.data(), data, size);
	assert(!compressed());
#if STATISTICS
	allocSize = size;
	globalAllocSize += allocSize;
	std::cout << "stat: DeltaBlockCopy " << globalAllocSize
	          << " (+" << allocSize << ')' << std::endl;
#endif
}

void DeltaBlockCopy::apply(uint8_t* dst, size_t size) const
{
	if (compressed()) {
		snappy::uncompress(
			reinterpret_cast<const char*>(block.data()), compressedSize,
			reinterpret_cast<char*>(dst), size);
	} else {
		memcpy(dst, block.data(), size);
	}
#ifdef DEBUG
	assert(SHA1::calc(dst, size) == sha1);
#endif
}

void DeltaBlockCopy::compress(size_t size)
{
	if (compressed()) return;

	size_t dstLen = snappy::maxCompressedLength(size);
	MemBuffer<uint8_t> buf2(dstLen);
	snappy::compress(reinterpret_cast<const char*>(block.data()), size,
	                 reinterpret_cast<char*>(buf2.data()), dstLen);
	if (dstLen >= size) {
		// compression isn't beneficial
		return;
	}
	compressedSize = dstLen;
	block.swap(buf2);
	block.resize(compressedSize); // shrink to fit
	assert(compressed());
#ifdef DEBUG
	MemBuffer<uint8_t> buf3(size);
	apply(buf3.data(), size);
	assert(memcmp(buf3.data(), buf2.data(), size) == 0);
#endif
#if STATISTICS
	int delta = compressedSize - allocSize;
	allocSize = compressedSize;
	globalAllocSize += delta;
	std::cout << "stat: compress " << globalAllocSize
	          << " (" << delta << ')' << std::endl;
#endif
}

const uint8_t* DeltaBlockCopy::getData()
{
	assert(!compressed());
	return block.data();
}


// class DeltaBlockDiff

DeltaBlockDiff::DeltaBlockDiff(
		const std::shared_ptr<DeltaBlockCopy>& prev_,
		const uint8_t* data, size_t size)
	: prev(prev_)
	, delta(calcDelta(prev->getData(), data, size))
{
#ifdef DEBUG
	sha1 = SHA1::calc(data, size);

	MemBuffer<uint8_t> buf(size);
	apply(buf.data(), size);
	assert(memcmp(buf.data(), data, size) == 0);
#endif
#if STATISTICS
	allocSize = delta.size();
	globalAllocSize += allocSize;
	std::cout << "stat: DeltaBlockDiff " << globalAllocSize
	          << " (+" << allocSize << ')' << std::endl;
#endif
}

void DeltaBlockDiff::apply(uint8_t* dst, size_t size) const
{
	prev->apply(dst, size);
	applyDeltaInPlace(dst, size, delta.data());
#ifdef DEBUG
	assert(SHA1::calc(dst, size) == sha1);
#endif
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
		if (ref) {
			// We will switch to a new DeltaBlockCopy object. So
			// now is a good time to compress the old one.
			ref->compress(size);
		}
		// Heuristic: create a new block when too many small
		// differences have accumulated.
		auto b = std::make_shared<DeltaBlockCopy>(data, size);
		it->ref = b;
		it->last = b;
		it->accSize = 0;
		return b;
	} else {
		// Create diff based on earlier reference block.
		// Reference remains unchanged.
		auto b = std::make_shared<DeltaBlockDiff>(ref, data, size);
		it->last = b;
		it->accSize += b->getDeltaSize();
		return b;
	}
}

std::shared_ptr<DeltaBlock> LastDeltaBlocks::createNullDiff(
		const void* id, const uint8_t* data, size_t size)
{
	auto it = std::lower_bound(begin(infos), end(infos), id,
		[](const Info& info, const void* id2) {
			return info.id < id2; });
	if ((it == end(infos)) || (it->id != id)) {
		// no previous block yet
		it = infos.emplace(it, id, size);
	}
	assert(it->size == size);

	auto last = it->last.lock();
	if (!last) {
		auto b = std::make_shared<DeltaBlockCopy>(data, size);
		it->ref = b;
		it->last = b;
		it->accSize = 0;
		return b;
	} else {
#ifdef DEBUG
		assert(SHA1::calc(data, size) == last->sha1);
#endif
		return last;
	}
}

void LastDeltaBlocks::clear()
{
	for (const Info& info : infos) {
		if (auto ref = info.ref.lock()) {
			ref->compress(info.size);
		}
	}
	infos.clear();
}

} // namespace openmsx
