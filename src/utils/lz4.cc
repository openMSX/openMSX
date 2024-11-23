#include "lz4.hh"

#include "aligned.hh"
#include "endian.hh"
#include "inline.hh"
#include "unreachable.hh"
#include <array>
#include <bit>
#include <cstring>

#ifdef _MSC_VER
#  include <intrin.h>
#  pragma warning(disable : 4127)  // disable: C4127: conditional expression is constant
#  pragma warning(disable : 4293)  // disable: C4293: too large shift (32-bits)
#endif

// 32 or 64 bits ?
#if (defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(__ppc64__) || defined(_WIN64) || defined(__LP64__) || defined(_LP64))
#  define LZ4_ARCH64 1
#else
#  define LZ4_ARCH64 0
#endif

namespace LZ4 {

static constexpr int MEMORY_USAGE = 14;
static constexpr int HASHLOG = MEMORY_USAGE - 2;
static constexpr int ACCELERATION = 1;
static constexpr int MINMATCH = 4;
static constexpr int WILDCOPYLENGTH = 8;
static constexpr int LASTLITERALS = 5; // see ../doc/lz4_Block_format.md#parsing-restrictions
static constexpr int MFLIMIT = 12;     // see ../doc/lz4_Block_format.md#parsing-restrictions
static constexpr int MATCH_SAFEGUARD_DISTANCE = 2 * WILDCOPYLENGTH - MINMATCH; // ensure it's possible to write 2 x wildcopyLength without overflowing output buffer
static constexpr int FASTLOOP_SAFE_DISTANCE = 64;
static constexpr int MIN_LENGTH = MFLIMIT + 1;
static constexpr int DISTANCE_MAX = 65535;
static constexpr int ML_BITS  = 4;
static constexpr int ML_MASK  = (1 << ML_BITS) - 1;
static constexpr int RUN_BITS = 8 - ML_BITS;
static constexpr int RUN_MASK = (1 << RUN_BITS) - 1;
static constexpr int LIMIT_64K = 0x10000 + (MFLIMIT - 1);
static constexpr uint32_t SKIP_TRIGGER = 6;  // Increase this value ==> compression run slower on incompressible data

using reg_t = uintptr_t;
static constexpr int STEPSIZE = sizeof(reg_t);


[[nodiscard]] static reg_t read_ARCH(const uint8_t* p)
{
	reg_t val;
	memcpy(&val, p, sizeof(val));
	return val;
}

// customized variant of memcpy, which can overwrite up to 8 bytes beyond dstEnd
static void wildCopy8(uint8_t* dst, const uint8_t* src, uint8_t* dstEnd)
{
	do {
		memcpy(dst, src, 8);
		dst += 8;
		src += 8;
	} while (dst < dstEnd);
}

// customized variant of memcpy, which can overwrite up to 32 bytes beyond dstEnd
// this version copies two times 16 bytes (instead of one time 32 bytes)
// because it must be compatible with offsets >= 16.
static void wildCopy32(uint8_t* dst, const uint8_t* src, uint8_t* dstEnd)
{
	do {
		memcpy(dst +  0, src +  0, 16);
		memcpy(dst + 16, src + 16, 16);
		dst += 32;
		src += 32;
	} while (dst < dstEnd);
}

static constexpr std::array<unsigned, 8> inc32table = {0, 1, 2,  1,  0,  4, 4, 4};
static constexpr std::array<int     , 8> dec64table = {0, 0, 0, -1, -4,  1, 2, 3};

static void memcpy_using_offset_base(uint8_t* dstPtr, const uint8_t* srcPtr, uint8_t* dstEnd, const size_t offset)
{
	if (offset < 8) {
		dstPtr[0] = srcPtr[0];
		dstPtr[1] = srcPtr[1];
		dstPtr[2] = srcPtr[2];
		dstPtr[3] = srcPtr[3];
		srcPtr += inc32table[offset];
		memcpy(dstPtr + 4, srcPtr, 4);
		srcPtr -= dec64table[offset];
		dstPtr += 8;
	} else {
		memcpy(dstPtr, srcPtr, 8);
		dstPtr += 8;
		srcPtr += 8;
	}

	wildCopy8(dstPtr, srcPtr, dstEnd);
}

// memcpy_using_offset()  presumes :
// - dstEnd >= dstPtr + MINMATCH
// - there is at least 8 bytes available to write after dstEnd
static void memcpy_using_offset(uint8_t* dstPtr, const uint8_t* srcPtr, uint8_t* dstEnd, size_t offset)
{
	std::array<uint8_t, 8> v;

	unalignedStore32(dstPtr, 0);   // silence an msan warning when offset==0

	switch (offset) {
		case 1:
			memset(v.data(), *srcPtr, 8);
			break;
		case 2:
			memcpy(&v[0], srcPtr, 2);
			memcpy(&v[2], srcPtr, 2);
			memcpy(&v[4], &v[0], 4);
			break;
		case 4:
			memcpy(&v[0], srcPtr, 4);
			memcpy(&v[4], srcPtr, 4);
			break;
		default:
			memcpy_using_offset_base(dstPtr, srcPtr, dstEnd, offset);
			return;
	}

	memcpy(dstPtr, v.data(), 8);
	dstPtr += 8;
	while (dstPtr < dstEnd) {
		memcpy(dstPtr, v.data(), 8);
		dstPtr += 8;
	}
}

[[nodiscard]] static inline int NbCommonBytes(size_t val)
{
	if (val == 0) UNREACHABLE;
	if constexpr (Endian::BIG) {
		return std::countl_zero(val) >> 3;
	} else {
		return std::countr_zero(val) >> 3;
	}
}

[[nodiscard]] static ALWAYS_INLINE unsigned count(const uint8_t* pIn, const uint8_t* pMatch, const uint8_t* pInLimit)
{
	const uint8_t* const pStart = pIn;

	if (pIn < pInLimit - (STEPSIZE - 1)) [[likely]] {
		reg_t diff = read_ARCH(pMatch) ^ read_ARCH(pIn);
		if (!diff) {
			pIn    += STEPSIZE;
			pMatch += STEPSIZE;
		} else {
			return NbCommonBytes(diff);
		}
	}
	while (pIn < pInLimit - (STEPSIZE - 1)) [[likely]] {
		reg_t diff = read_ARCH(pMatch) ^ read_ARCH(pIn);
		if (!diff) {
			pIn    += STEPSIZE;
			pMatch += STEPSIZE;
			continue;
		}
		pIn += NbCommonBytes(diff);
		return unsigned(pIn - pStart);
	}

	if ((STEPSIZE == 8) && (pIn < (pInLimit - 3)) && (unalignedLoad32(pMatch) == unalignedLoad32(pIn))) {
		pIn    += 4;
		pMatch += 4;
	}
	if ((pIn < (pInLimit - 1)) && (unalignedLoad16(pMatch) == unalignedLoad16(pIn))) {
		pIn += 2;
		pMatch += 2;
	}
	if ((pIn < pInLimit) && (*pMatch == *pIn)) {
		pIn += 1;
	}
	return unsigned(pIn - pStart);
}


template<bool L64K, bool ARCH64> struct HashImpl;

// byU16
template<bool ARCH64> struct HashImpl<true, ARCH64> {
	alignas(uint64_t) std::array<uint16_t, 1 << (HASHLOG + 1)> tab = {};

	[[nodiscard]] static uint32_t hashPosition(const uint8_t* p) {
		uint32_t sequence = unalignedLoad32(p);
		return (sequence * 2654435761U) >> ((MINMATCH * 8) - (HASHLOG + 1));
	}
	void putIndexOnHash(uint32_t idx, uint32_t h) {
		tab[h] = uint16_t(idx);
	}
	void putPositionOnHash(const uint8_t* p, uint32_t h, const uint8_t* srcBase) {
		tab[h] = uint16_t(p - srcBase);
	}
	void putPosition(const uint8_t* p, const uint8_t* srcBase) {
		putPositionOnHash(p, hashPosition(p), srcBase);
	}
	[[nodiscard]] uint32_t getIndexOnHash(uint32_t h) const {
		return tab[h];
	}
	[[nodiscard]] const uint8_t* getPositionOnHash(uint32_t h, const uint8_t* srcBase) const {
		return tab[h] + srcBase;
	}
	[[nodiscard]] const uint8_t* getPosition(const uint8_t* p, const uint8_t* srcBase) const {
		return getPositionOnHash(hashPosition(p), srcBase);
	}
};

// byU32
template<> struct HashImpl<false, true> {
	alignas(uint64_t) std::array<uint32_t, 1 << HASHLOG> tab = {};

	[[nodiscard]] static uint32_t hashPosition(const uint8_t* p) {
		uint64_t sequence = read_ARCH(p);
		const uint64_t prime = Endian::BIG
				     ? 11400714785074694791ULL   // 8 bytes
				     :         889523592379ULL;  // 5 bytes
		return uint32_t(((sequence << 24) * prime) >> (64 - HASHLOG));
	}
	void putIndexOnHash(uint32_t idx, uint32_t h) {
		tab[h] = idx;
	}
	void putPositionOnHash(const uint8_t* p, uint32_t h, const uint8_t* srcBase) {
		tab[h] = uint32_t(p - srcBase);
	}
	void putPosition(const uint8_t* p, const uint8_t* srcBase) {
		putPositionOnHash(p, hashPosition(p), srcBase);
	}
	[[nodiscard]] uint32_t getIndexOnHash(uint32_t h) const {
		return tab[h];
	}
	[[nodiscard]] const uint8_t* getPositionOnHash(uint32_t h, const uint8_t* srcBase) const {
		return tab[h] + srcBase;
	}
	[[nodiscard]] const uint8_t* getPosition(const uint8_t* p, const uint8_t* srcBase) const {
		return getPositionOnHash(hashPosition(p), srcBase);
	}
};

// byPtr
template<> struct HashImpl<false, false> {
	alignas(uint64_t) std::array<const uint8_t*, 1 << HASHLOG> tab = {};

	[[nodiscard]] static uint32_t hashPosition(const uint8_t* p) {
		uint32_t sequence = unalignedLoad32(p);
		return (sequence * 2654435761U) >> ((MINMATCH * 8) - HASHLOG);
	}
	void putIndexOnHash(uint32_t /*idx*/, uint32_t /*h*/) {
		UNREACHABLE;
	}
	void putPositionOnHash(const uint8_t* p, uint32_t h, const uint8_t* /*srcBase*/) {
		tab[h] = p;
	}
	void putPosition(const uint8_t* p, const uint8_t* srcBase) {
		putPositionOnHash(p, hashPosition(p), srcBase);
	}
	[[nodiscard]] uint32_t getIndexOnHash(uint32_t /*h*/) const {
		UNREACHABLE;
	}
	[[nodiscard]] const uint8_t* getPositionOnHash(uint32_t h, const uint8_t* /*srcBase*/) const {
		return tab[h];
	}
	[[nodiscard]] const uint8_t* getPosition(const uint8_t* p, const uint8_t* srcBase) const {
		return getPositionOnHash(hashPosition(p), srcBase);
	}
};

template<bool L64K, bool ARCH64>
static ALWAYS_INLINE int compress_impl(const uint8_t* src, uint8_t* const dst, const int inputSize)
{
	HashImpl<L64K, ARCH64> hashTable;

	const uint8_t* ip = src;
	uint8_t* op = dst;

	const uint8_t* anchor = src;
	const uint8_t* const iend = ip + inputSize;
	const uint8_t* const mflimitPlusOne = iend - MFLIMIT + 1;
	const uint8_t* const matchlimit = iend - LASTLITERALS;

	uint32_t forwardH;

	if (inputSize < MIN_LENGTH) goto _last_literals; // Input too small, no compression (all literals)

	// First byte
	hashTable.putPosition(ip, src);
	ip++;
	forwardH = hashTable.hashPosition(ip);

	while (true) {
		// Find a match
		const uint8_t* match;
		if constexpr (!L64K && !ARCH64) { // byPtr
			const uint8_t* forwardIp = ip;
			int step = 1;
			int searchMatchNb = ACCELERATION << SKIP_TRIGGER;
			do {
				uint32_t h = forwardH;
				ip = forwardIp;
				forwardIp += step;
				step = searchMatchNb++ >> SKIP_TRIGGER;

				if (forwardIp > mflimitPlusOne) [[unlikely]] goto _last_literals;

				match = hashTable.getPositionOnHash(h, src);
				forwardH = hashTable.hashPosition(forwardIp);
				hashTable.putPositionOnHash(ip, h, src);
			} while ((match + DISTANCE_MAX < ip) ||
			         (unalignedLoad32(match) != unalignedLoad32(ip)));

		} else { // byU16 or byU32
			const uint8_t* forwardIp = ip;
			int step = 1;
			int searchMatchNb = ACCELERATION << SKIP_TRIGGER;
			while (true) {
				auto h = forwardH;
				auto current = uint32_t(forwardIp - src);
				auto matchIndex = hashTable.getIndexOnHash(h);
				ip = forwardIp;
				forwardIp += step;
				step = searchMatchNb++ >> SKIP_TRIGGER;

				if (forwardIp > mflimitPlusOne) [[unlikely]] goto _last_literals;

				match = src + matchIndex;
				forwardH = hashTable.hashPosition(forwardIp);
				hashTable.putIndexOnHash(current, h);

				if (!L64K && (matchIndex + DISTANCE_MAX < current)) {
					continue; // too far
				}

				if (unalignedLoad32(match) == unalignedLoad32(ip)) {
					break; // match found
				}
			}
		}

		// Catch up
		while (((ip > anchor) & (match > src)) && (/*unlikely*/(ip[-1] == match[-1]))) {
			ip--;
			match--;
		}

		// Encode Literals
		auto litLength = unsigned(ip - anchor);
		uint8_t* token = op++;
		if (litLength >= RUN_MASK) {
			auto len = int(litLength - RUN_MASK);
			*token = RUN_MASK << ML_BITS;
			while (len >= 255) {
				*op++ = 255;
				len -= 255;
			}
			*op++ = uint8_t(len);
		} else {
			*token = uint8_t(litLength << ML_BITS);
		}

		// Copy Literals
		wildCopy8(op, anchor, op + litLength);
		op += litLength;

_next_match:
		// At this stage, the following variables must be correctly set:
		// - ip : at start of LZ operation
		// - match : at start of previous pattern occurrence; can be within current prefix, or within extDict
		// - token and *token : position to write 4-bits for match length; higher 4-bits for literal length supposed already written

		// Encode Offset
		Endian::write_UA_L16(op, uint16_t(ip - match));
		op += 2;

		// Encode MatchLength
		unsigned matchCode = count(ip + MINMATCH, match + MINMATCH, matchlimit);
		ip += size_t(matchCode) + MINMATCH;

		if (matchCode >= ML_MASK) {
			*token += ML_MASK;
			matchCode -= ML_MASK;
			unalignedStore32(op, 0xFFFFFFFF);
			while (matchCode >= 4 * 255) {
				op += 4;
				unalignedStore32(op, 0xFFFFFFFF);
				matchCode -= 4 * 255;
			}
			op += matchCode / 255;
			*op++ = uint8_t(matchCode % 255);
		} else {
			*token += uint8_t(matchCode);
		}

		anchor = ip;

		// Test end of chunk
		if (ip >= mflimitPlusOne) break;

		// Fill table
		hashTable.putPosition(ip - 2, src);

		// Test next position
		if constexpr (!L64K && !ARCH64) { // byPtr
			match = hashTable.getPosition(ip, src);
			hashTable.putPosition(ip, src);
			if ((match + DISTANCE_MAX >= ip) && (unalignedLoad32(match) == unalignedLoad32(ip))) {
				token = op++;
				*token = 0;
				goto _next_match;
			}
		} else { // byU16 or byU32
			auto h = hashTable.hashPosition(ip);
			auto current = uint32_t(ip - src);
			auto matchIndex = hashTable.getIndexOnHash(h);
			match = src + matchIndex;
			hashTable.putIndexOnHash(current, h);
			if ((L64K || (matchIndex + DISTANCE_MAX >= current)) &&
			    (unalignedLoad32(match) == unalignedLoad32(ip))) {
				token = op++;
				*token = 0;
				goto _next_match;
			}
		}

		// Prepare next loop
		forwardH = hashTable.hashPosition(++ip);
	}

_last_literals:
	// Encode Last Literals
	auto lastRun = size_t(iend - anchor);
	if (lastRun >= RUN_MASK) {
		size_t accumulator = lastRun - RUN_MASK;
		*op++ = RUN_MASK << ML_BITS;
		while (accumulator >= 255) {
			*op++ = 255;
			accumulator -= 255;
		}
		*op++ = uint8_t(accumulator);
	} else {
		*op++ = uint8_t(lastRun << ML_BITS);
	}
	memcpy(op, anchor, lastRun);
	ip = anchor + lastRun;
	op += lastRun;

	return int(op - dst);
}

int compress(const uint8_t* src, uint8_t* dst, int srcSize)
{
	if (srcSize < LIMIT_64K) {
		return compress_impl<true, LZ4_ARCH64>(src, dst, srcSize);
	} else {
		return compress_impl<false, LZ4_ARCH64>(src, dst, srcSize);
	}
}



static ALWAYS_INLINE unsigned read_variable_length(const uint8_t** ip)
{
	unsigned length = 0;
	unsigned s;
	do {
		s = **ip;
		(*ip)++;
		length += s;
	} while (s == 255);

	return length;
}

int decompress(const uint8_t* src, uint8_t* dst, int compressedSize, int dstCapacity)
{
	const uint8_t* ip = src;
	const uint8_t* const iend = ip + compressedSize;

	uint8_t* op = dst;
	uint8_t* const oend = op + dstCapacity;
	uint8_t* cpy;

	// Set up the "end" pointers for the shortcut.
	const uint8_t* const shortiend = iend - 14 /*maxLL*/ -  2 /*offset*/;
	const uint8_t* const shortoend = oend - 14 /*maxLL*/ - 18 /*maxML*/;

	const uint8_t* match;
	size_t offset;
	unsigned token;
	size_t length;

	if ((oend - op) >= FASTLOOP_SAFE_DISTANCE) {
		// Fast loop : decode sequences as long as output < iend-FASTLOOP_SAFE_DISTANCE
		while (true) {
			// Main fastloop assertion: We can always wildcopy FASTLOOP_SAFE_DISTANCE
			token = *ip++;
			length = token >> ML_BITS; // literal length

			// decode literal length
			if (length == RUN_MASK) {
				length += read_variable_length(&ip);

				// copy literals
				cpy = op + length;
				if ((cpy > oend - 32) || (ip + length > iend - 32)) {
					goto safe_literal_copy;
				}
				wildCopy32(op, ip, cpy);
				ip += length;
				op = cpy;
			} else {
				cpy = op + length;
				// We don't need to check oend, since we check it once for each loop below
				if (ip > iend - (16 + 1/*max lit + offset + nextToken*/)) {
					goto safe_literal_copy;
				}
				// Literals can only be 14, but hope compilers optimize if we copy by a register size
				memcpy(op, ip, 16);
				ip += length;
				op = cpy;
			}

			// get offset
			offset = Endian::read_UA_L16(ip);
			ip += 2;
			match = op - offset;

			// get match-length
			length = token & ML_MASK;

			if (length == ML_MASK) {
				length += read_variable_length(&ip);
				length += MINMATCH;
				if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
					goto safe_match_copy;
				}
			} else {
				length += MINMATCH;
				if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
					goto safe_match_copy;
				}

				// Fast path check: Avoids a branch in wildCopy32 if true
				if ((match >= dst) && (offset >= 8)) {
					memcpy(op +  0, match +  0, 8);
					memcpy(op +  8, match +  8, 8);
					memcpy(op + 16, match + 16, 2);
					op += length;
					continue;
				}
			}

			// copy match within block
			cpy = op + length;

			if (offset < 16) [[unlikely]] {
				memcpy_using_offset(op, match, cpy, offset);
			} else {
				wildCopy32(op, match, cpy);
			}

			op = cpy; // wildcopy correction
		}
	}

	// Main Loop : decode remaining sequences where output < FASTLOOP_SAFE_DISTANCE
	while (true) {
		token = *ip++;
		length = token >> ML_BITS; // literal length

		// A two-stage shortcut for the most common case:
		// 1) If the literal length is 0..14, and there is enough space,
		// enter the shortcut and copy 16 bytes on behalf of the literals
		// (in the fast mode, only 8 bytes can be safely copied this way).
		// 2) Further if the match length is 4..18, copy 18 bytes in a similar
		// manner; but we ensure that there's enough space in the output for
		// those 18 bytes earlier, upon entering the shortcut (in other words,
		// there is a combined check for both stages).
		if ((length != RUN_MASK) &&
		    // strictly "less than" on input, to re-enter the loop with at least one byte
		    /*likely*/((ip < shortiend) & (op <= shortoend))) {
			// Copy the literals
			memcpy(op, ip, 16);
			op += length;
			ip += length;

			// The second stage: prepare for match copying, decode full info.
			// If it doesn't work out, the info won't be wasted.
			length = token & ML_MASK; // match length
			offset = Endian::read_UA_L16(ip);
			ip += 2;
			match = op - offset;

			// Do not deal with overlapping matches.
			if ((length != ML_MASK) && (offset >= 8) && (match >= dst)) {
				// Copy the match.
				memcpy(op +  0, match +  0, 8);
				memcpy(op +  8, match +  8, 8);
				memcpy(op + 16, match + 16, 2);
				op += length + MINMATCH;
				// Both stages worked, load the next token.
				continue;
			}

			// The second stage didn't work out, but the info is ready.
			// Propel it right to the point of match copying.
			goto _copy_match;
		}

		// decode literal length
		if (length == RUN_MASK) {
			length += read_variable_length(&ip);
		}

		// copy literals
		cpy = op + length;
safe_literal_copy:
		if ((cpy > oend - MFLIMIT) || (ip + length > iend - (2 + 1 + LASTLITERALS))) {
			// We've either hit the input parsing restriction or the output parsing restriction.
			// If we've hit the input parsing condition then this must be the last sequence.
			// If we've hit the output parsing condition then we are either using partialDecoding
			// or we've hit the output parsing condition.
			memmove(op, ip, length); // supports overlapping memory regions, which only matters for in-place decompression scenarios
			ip += length;
			op += length;
			break;
		} else {
			wildCopy8(op, ip, cpy); // may overwrite up to WILDCOPYLENGTH beyond cpy
			ip += length;
			op = cpy;
		}

		// get offset
		offset = Endian::read_UA_L16(ip);
		ip += 2;
		match = op - offset;

		// get matchlength
		length = token & ML_MASK;

_copy_match:
		if (length == ML_MASK) {
			length += read_variable_length(&ip);
		}
		length += MINMATCH;

safe_match_copy:
		// copy match within block
		cpy = op + length;

		if (offset < 8) [[unlikely]] {
			unalignedStore32(op, 0); // silence msan warning when offset == 0
			op[0] = match[0];
			op[1] = match[1];
			op[2] = match[2];
			op[3] = match[3];
			match += inc32table[offset];
			memcpy(op + 4, match, 4);
			match -= dec64table[offset];
		} else {
			memcpy(op, match, 8);
			match += 8;
		}
		op += 8;

		if (cpy > oend - MATCH_SAFEGUARD_DISTANCE) [[unlikely]] {
			uint8_t* const oCopyLimit = oend - (WILDCOPYLENGTH - 1);
			if (op < oCopyLimit) {
				wildCopy8(op, match, oCopyLimit);
				match += oCopyLimit - op;
				op = oCopyLimit;
			}
			while (op < cpy) {
				*op++ = *match++;
			}
		} else {
			memcpy(op, match, 8);
			if (length > 16)  {
				wildCopy8(op + 8, match + 8, cpy);
			}
		}
		op = cpy; // wildcopy correction
	}

	return int(op - dst); // Nb of output bytes decoded
}

} // namespace LZ4
