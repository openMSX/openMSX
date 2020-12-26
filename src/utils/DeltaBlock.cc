#include "DeltaBlock.hh"
#include "likely.hh"
#include "ranges.hh"
#include "lz4.hh"
#include <cassert>
#include <cstring>
#include <tuple>
#include <utility>
#if STATISTICS
#include <iostream>
#endif
#ifdef __SSE2__
#include <emmintrin.h>
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

[[nodiscard]] static size_t loadUleb(const uint8_t*& data)
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


// --- Helper functions to compare {4,8,16} bytes at aligned memory locations ---

template<int N> bool comp(const uint8_t* p, const uint8_t* q);

template<> bool comp<4>(const uint8_t* p, const uint8_t* q)
{
	return *reinterpret_cast<const uint32_t*>(p) ==
	       *reinterpret_cast<const uint32_t*>(q);
}

template<> bool comp<8>(const uint8_t* p, const uint8_t* q)
{
	return *reinterpret_cast<const uint64_t*>(p) ==
	       *reinterpret_cast<const uint64_t*>(q);
}

#ifdef __SSE2__
template<> bool comp<16>(const uint8_t* p, const uint8_t* q)
{
	// Tests show that (on my machine) using 1 128-bit load is faster than
	// 2 64-bit loads. Even though the actual comparison is slightly more
	// complicated with SSE instructions.
	__m128i a = _mm_load_si128(reinterpret_cast<const __m128i*>(p));
	__m128i b = _mm_load_si128(reinterpret_cast<const __m128i*>(q));
	__m128i d = _mm_cmpeq_epi8(a, b);
	return _mm_movemask_epi8(d) == 0xffff;
}
#endif


// --- Optimized mismatch function ---

// This is much like the function std::mismatch(). You pass in two buffers,
// the corresponding elements of both buffers are compared and the first
// position where the elements no longer match is returned.
//
// Compared to the std::mismatch() this implementation is faster because:
// - We make use of sentinels. This requires to temporarily change the content
//   of the buffer. So it won't work with read-only-memory.
// - We compare words-at-a-time instead of byte-at-a-time.
static std::pair<const uint8_t*, const uint8_t*> scan_mismatch(
	const uint8_t* p, const uint8_t* p_end, const uint8_t* q, const uint8_t* q_end)
{
	assert((p_end - p) == (q_end - q));

	// When SSE is available, work with 16-byte words, otherwise 4 or 8
	// bytes. This routine also benefits from AVX2 instructions. Though not
	// all x86_64 CPUs have AVX2 (all have SSE2), so using them requires
	// extra run-time checks and that's not worth it at this point.
	constexpr int WORD_SIZE =
#ifdef __SSE2__
		sizeof(__m128i);
#else
		sizeof(void*);
#endif

	// Region too small or
	// both buffers are differently aligned.
	if (unlikely((p_end - p) < (2 * WORD_SIZE)) ||
	    unlikely((reinterpret_cast<uintptr_t>(p) & (WORD_SIZE - 1)) !=
	             (reinterpret_cast<uintptr_t>(q) & (WORD_SIZE - 1)))) {
		goto end;
	}

	// Align to WORD_SIZE boundary. No need for end-of-buffer checks.
	if (unlikely(reinterpret_cast<uintptr_t>(p) & (WORD_SIZE - 1))) {
		do {
			if (*p != *q) return {p, q};
			p += 1; q += 1;
		} while (reinterpret_cast<uintptr_t>(p) & (WORD_SIZE - 1));
	}

	// Fast path. Compare words-at-a-time.
	{
		// Place a sentinel in the last full word. This ensures we'll
		// find a mismatch within the buffer, and so we can omit the
		// end-of-buffer checks.
		auto* sentinel = &const_cast<uint8_t*>(p_end)[-WORD_SIZE];
		auto save = *sentinel;
		*sentinel = ~q_end[-WORD_SIZE];

		while (comp<WORD_SIZE>(p, q)) {
			p += WORD_SIZE; q += WORD_SIZE;
		}

		// Restore sentinel.
		*sentinel = save;
	}

	// Slow path. This handles:
	// - Small or differently aligned buffers.
	// - The bytes at and after the (restored) sentinel.
end:	return std::mismatch(p, p_end, q);
}


// --- Optimized scan_match function ---

// Like scan_mismatch() above, but searches two buffers for the first
// corresponding equal (instead of not-equal) bytes.
//
// Like scan_mismatch(), this places a temporary sentinel in the buffer, so the
// buffer cannot be read-only memory.
//
// Unlike scan_mismatch() it's less obvious how to perform this function
// word-at-a-time (it's possible with some bit hacks). Though luckily this
// function is also less performance critical.
[[nodiscard]] static std::pair<const uint8_t*, const uint8_t*> scan_match(
	const uint8_t* p, const uint8_t* p_end, const uint8_t* q, const uint8_t* q_end)
{
	assert((p_end - p) == (q_end - q));

	// Code below is functionally equivalent to:
	//   while ((p != p_end) && (*p != *q)) { ++p; ++q; }
	//   return {p, q};

	if (p == p_end) return {p, q};

	auto* p_last = const_cast<uint8_t*>(p_end - 1);
	auto save = *p_last;
	*p_last = q_end[-1]; // make p_end[-1] == q_end[-1]

	while (*p != *q) { ++p; ++q; }

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
[[nodiscard]] static vector<uint8_t> calcDelta(
	const uint8_t* oldBuf, const uint8_t* newBuf, size_t size)
{
	vector<uint8_t> result;

	const auto* p = oldBuf;
	const auto* q = newBuf;
	const auto* p_end = p + size;
	const auto* q_end = q + size;

	// scan equal bytes (possibly zero)
	const auto* q1 = q;
	std::tie(p, q) = scan_mismatch(p, p_end, q, q_end);
	auto n1 = q - q1;
	storeUleb(result, n1);

	while (q != q_end) {
		assert(*p != *q);

		const auto* q2 = q;
	different:
		std::tie(p, q) = scan_match(p + 1, p_end, q + 1, q_end);
		auto n2 = q - q2;

		const auto* q3 = q;
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

DeltaBlock::~DeltaBlock()
{
	globalAllocSize -= allocSize;
	std::cout << "stat: ~DeltaBlock " << globalAllocSize
	          << " (-" << allocSize << ")\n";
}

#endif

// class DeltaBlockCopy

DeltaBlockCopy::DeltaBlockCopy(const uint8_t* data, size_t size)
	: block(size)
	, compressedSize(0)
{
#ifdef DEBUG
	sha1 = SHA1::calc({data, size});
#endif
	memcpy(block.data(), data, size);
	assert(!compressed());
#if STATISTICS
	allocSize = size;
	globalAllocSize += allocSize;
	std::cout << "stat: DeltaBlockCopy " << globalAllocSize
	          << " (+" << allocSize << ")\n";
#endif
}

void DeltaBlockCopy::apply(uint8_t* dst, size_t size) const
{
	if (compressed()) {
		LZ4::decompress(block.data(), dst, int(compressedSize), int(size));
	} else {
		memcpy(dst, block.data(), size);
	}
#ifdef DEBUG
	assert(SHA1::calc({dst, size}) == sha1);
#endif
}

void DeltaBlockCopy::compress(size_t size)
{
	if (compressed()) return;

	size_t dstLen = LZ4::compressBound(size);
	MemBuffer<uint8_t> buf2(dstLen);
	dstLen = LZ4::compress(block.data(), buf2.data(), int(size));

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
	          << " (" << delta << ")\n";
#endif
}

const uint8_t* DeltaBlockCopy::getData()
{
	assert(!compressed());
	return block.data();
}


// class DeltaBlockDiff

DeltaBlockDiff::DeltaBlockDiff(
		std::shared_ptr<DeltaBlockCopy> prev_,
		const uint8_t* data, size_t size)
	: prev(std::move(prev_))
	, delta(calcDelta(prev->getData(), data, size))
{
#ifdef DEBUG
	sha1 = SHA1::calc({data, size});

	MemBuffer<uint8_t> buf(size);
	apply(buf.data(), size);
	assert(memcmp(buf.data(), data, size) == 0);
#endif
#if STATISTICS
	allocSize = delta.size();
	globalAllocSize += allocSize;
	std::cout << "stat: DeltaBlockDiff " << globalAllocSize
	          << " (+" << allocSize << ")\n";
#endif
}

void DeltaBlockDiff::apply(uint8_t* dst, size_t size) const
{
	prev->apply(dst, size);
	applyDeltaInPlace(dst, size, delta.data());
#ifdef DEBUG
	assert(SHA1::calc({dst, size}) == sha1);
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
	auto it = ranges::lower_bound(infos, std::tuple(id, size),
		[](const Info& info, const std::tuple<const void*, size_t>& info2) {
			return std::tuple(info.id, info.size) < info2; });
	if ((it == end(infos)) || (it->id != id) || (it->size != size)) {
		// no previous info yet
		it = infos.emplace(it, id, size);
	}
	assert(it->id   == id);
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
	auto it = ranges::lower_bound(infos, id,
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
		assert(SHA1::calc({data, size}) == last->sha1);
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
