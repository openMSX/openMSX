#include "snappy.hh"
#include "aligned.hh"
#include "likely.hh"
#include "endian.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdint>

using namespace openmsx;

namespace snappy {

enum {
	LITERAL = 0,
	COPY_1_BYTE_OFFSET = 1,  // 3 bit length + 3 bits of offset in opcode
	COPY_2_BYTE_OFFSET = 2,
	COPY_4_BYTE_OFFSET = 3
};

static inline void unalignedCopy64(const char* src, char* dst)
{
	if (sizeof(void*) == 8) {
		unalignedStore64(dst + 0, unalignedLoad64(src + 0));
	} else {
		// This can be more efficient than unalignedLoad64 +
		// unalignedStore64 on some platforms, in particular ARM.
		unalignedStore32(dst + 0, unalignedLoad32(src + 0));
		unalignedStore32(dst + 4, unalignedLoad32(src + 4));
	}
}
static inline void unalignedCopy128(const char* src, char* dst)
{
	unalignedCopy64(src + 0, dst + 0);
	unalignedCopy64(src + 8, dst + 8);
}

/////////////////

// Copy "len" bytes from "src" to "op", one byte at a time.  Used for
// handling COPY operations where the input and output regions may
// overlap.  For example, suppose:
//    src    == "ab"
//    op     == src + 2
//    len    == 20
// After incrementalCopy(src, op, len), the result will have
// eleven copies of "ab"
//    ababababababababababab
// Note that this does not match the semantics of either memcpy()
// or memmove().
static inline void incrementalCopy(const char* src, char* op, ptrdiff_t len)
{
	assert(len > 0);
	do {
		*op++ = *src++;
	} while (--len);
}

// Equivalent to incrementalCopy() except that it can write up to ten extra
// bytes after the end of the copy, and that it is faster.
//
// The main part of this loop is a simple copy of eight bytes at a time until
// we've copied (at least) the requested amount of bytes.  However, if op and
// src are less than eight bytes apart (indicating a repeating pattern of
// length < 8), we first need to expand the pattern in order to get the correct
// results. For instance, if the buffer looks like this, with the eight-byte
// <src> and <op> patterns marked as intervals:
//
//    abxxxxxxxxxxxx
//    [------]           src
//      [------]         op
//
// a single eight-byte copy from <src> to <op> will repeat the pattern once,
// after which we can move <op> two bytes without moving <src>:
//
//    ababxxxxxxxxxx
//    [------]           src
//        [------]       op
//
// and repeat the exercise until the two no longer overlap.
//
// This allows us to do very well in the special case of one single byte
// repeated many times, without taking a big hit for more general cases.
//
// The worst case of extra writing past the end of the match occurs when
// op - src == 1 and len == 1; the last copy will read from byte positions
// [0..7] and write to [4..11], whereas it was only supposed to write to
// position 1. Thus, ten excess bytes.

static const int MAX_INCR_COPY_OVERFLOW = 10;

static inline void incrementalCopyFast(const char* src, char* op, ptrdiff_t len)
{
	while (op - src < 8) {
		unalignedCopy64(src, op);
		len -= op - src;
		op += op - src;
	}
	while (len > 0) {
		unalignedCopy64(src, op);
		src += 8;
		op += 8;
		len -= 8;
	}
}

static inline uint32_t loadNBytes(const void* p, unsigned n)
{
	// Mapping from i in range [0,4] to a mask to extract the bottom i bytes
	static const uint32_t wordmask[] = {
		0, 0xff, 0xffff, 0xffffff, 0xffffffff
	};
	return Endian::read_UA_L32(p) & wordmask[n];
}

// Data stored per entry in lookup table:
//      Range   Bits-used       Description
//      ------------------------------------
//      1..64   0..7            Literal/copy length encoded in opcode byte
//      0..7    8..10           Copy offset encoded in opcode byte / 256
//      0..4    11..13          Extra bytes after opcode
//
// We use eight bits for the length even though 7 would have sufficed
// because of efficiency reasons:
//      (1) Extracting a byte is faster than a bit-field
//      (2) It properly aligns copy offset so we do not need a <<8
// See original code for a generator for this table.
static const uint16_t charTable[256] = {
	0x0001, 0x0804, 0x1001, 0x2001, 0x0002, 0x0805, 0x1002, 0x2002,
	0x0003, 0x0806, 0x1003, 0x2003, 0x0004, 0x0807, 0x1004, 0x2004,
	0x0005, 0x0808, 0x1005, 0x2005, 0x0006, 0x0809, 0x1006, 0x2006,
	0x0007, 0x080a, 0x1007, 0x2007, 0x0008, 0x080b, 0x1008, 0x2008,
	0x0009, 0x0904, 0x1009, 0x2009, 0x000a, 0x0905, 0x100a, 0x200a,
	0x000b, 0x0906, 0x100b, 0x200b, 0x000c, 0x0907, 0x100c, 0x200c,
	0x000d, 0x0908, 0x100d, 0x200d, 0x000e, 0x0909, 0x100e, 0x200e,
	0x000f, 0x090a, 0x100f, 0x200f, 0x0010, 0x090b, 0x1010, 0x2010,
	0x0011, 0x0a04, 0x1011, 0x2011, 0x0012, 0x0a05, 0x1012, 0x2012,
	0x0013, 0x0a06, 0x1013, 0x2013, 0x0014, 0x0a07, 0x1014, 0x2014,
	0x0015, 0x0a08, 0x1015, 0x2015, 0x0016, 0x0a09, 0x1016, 0x2016,
	0x0017, 0x0a0a, 0x1017, 0x2017, 0x0018, 0x0a0b, 0x1018, 0x2018,
	0x0019, 0x0b04, 0x1019, 0x2019, 0x001a, 0x0b05, 0x101a, 0x201a,
	0x001b, 0x0b06, 0x101b, 0x201b, 0x001c, 0x0b07, 0x101c, 0x201c,
	0x001d, 0x0b08, 0x101d, 0x201d, 0x001e, 0x0b09, 0x101e, 0x201e,
	0x001f, 0x0b0a, 0x101f, 0x201f, 0x0020, 0x0b0b, 0x1020, 0x2020,
	0x0021, 0x0c04, 0x1021, 0x2021, 0x0022, 0x0c05, 0x1022, 0x2022,
	0x0023, 0x0c06, 0x1023, 0x2023, 0x0024, 0x0c07, 0x1024, 0x2024,
	0x0025, 0x0c08, 0x1025, 0x2025, 0x0026, 0x0c09, 0x1026, 0x2026,
	0x0027, 0x0c0a, 0x1027, 0x2027, 0x0028, 0x0c0b, 0x1028, 0x2028,
	0x0029, 0x0d04, 0x1029, 0x2029, 0x002a, 0x0d05, 0x102a, 0x202a,
	0x002b, 0x0d06, 0x102b, 0x202b, 0x002c, 0x0d07, 0x102c, 0x202c,
	0x002d, 0x0d08, 0x102d, 0x202d, 0x002e, 0x0d09, 0x102e, 0x202e,
	0x002f, 0x0d0a, 0x102f, 0x202f, 0x0030, 0x0d0b, 0x1030, 0x2030,
	0x0031, 0x0e04, 0x1031, 0x2031, 0x0032, 0x0e05, 0x1032, 0x2032,
	0x0033, 0x0e06, 0x1033, 0x2033, 0x0034, 0x0e07, 0x1034, 0x2034,
	0x0035, 0x0e08, 0x1035, 0x2035, 0x0036, 0x0e09, 0x1036, 0x2036,
	0x0037, 0x0e0a, 0x1037, 0x2037, 0x0038, 0x0e0b, 0x1038, 0x2038,
	0x0039, 0x0f04, 0x1039, 0x2039, 0x003a, 0x0f05, 0x103a, 0x203a,
	0x003b, 0x0f06, 0x103b, 0x203b, 0x003c, 0x0f07, 0x103c, 0x203c,
	0x0801, 0x0f08, 0x103d, 0x203d, 0x1001, 0x0f09, 0x103e, 0x203e,
	0x1801, 0x0f0a, 0x103f, 0x203f, 0x2001, 0x0f0b, 0x1040, 0x2040
};

static const size_t SCRATCH_SIZE = 16;

void uncompress(const char* input, size_t inLen,
                char* output, size_t outLen)
{
	const char* ip = input;
	const char* ipLimit = input + inLen - SCRATCH_SIZE;
	char* op = output;;
	char* opLimit = output + outLen;

	while (ip != ipLimit) {
		unsigned char c = *ip++;
		if ((c & 0x3) == LITERAL) {
			size_t literalLen = (c >> 2) + 1;
			size_t outLeft = opLimit - op;
			if (literalLen <= 16 && outLeft >= 16) {
				// Fast path, used for the majority (about 95%)
				// of invocations.
				unalignedCopy128(ip, op);
				op += literalLen;
				ip += literalLen;
				continue;
			}
			if (unlikely(literalLen >= 61)) {
				// Long literal.
				size_t literalLenLen = literalLen - 60;
				literalLen = loadNBytes(ip, unsigned(literalLenLen)) + 1;
				ip += literalLenLen;
			}
			memcpy(op, ip, literalLen);
			op += literalLen;
			ip += literalLen;
		} else {
			uint32_t entry = charTable[c];
			uint32_t trailer = loadNBytes(ip, entry >> 11);
			uint32_t length = entry & 0xff;
			ip += entry >> 11;

			// offset/256 is encoded in bits 8..10. By just
			// fetching those bits, we get copyOffset (since the
			// bit-field starts at bit 8).
			size_t offset = (entry & 0x700) + trailer;
			size_t outLeft = opLimit - op;
			const char* src = op - offset;
			if (length <= 16 && offset >= 8 && outLeft >= 16) {
				// Fast path, used for the majority (70-80%) of
				// dynamic invocations.
				unalignedCopy128(src, op);
			} else {
				if (outLeft >= length + MAX_INCR_COPY_OVERFLOW) {
					incrementalCopyFast(src, op, length);
				} else {
					incrementalCopy(src, op, length);
				}
			}
			op += length;
		}
	}
}


/////////////////

// Any hash function will produce a valid compressed bitstream, but a good hash
// function reduces the number of collisions and thus yields better compression
// for compressible input, and more speed for incompressible input. Of course,
// it doesn't hurt if the hash function is reasonably fast either, as it gets
// called a lot.
template<int SHIFT> static inline uint32_t hashBytes(uint32_t bytes)
{
	return (bytes * 0x1e35a7bd) >> SHIFT;
}
template<int SHIFT> static inline uint32_t hash(const char* p)
{
	return hashBytes<SHIFT>(unalignedLoad32(p));
}

// For 0 <= offset <= 4, getUint32AtOffset(getEightBytesAt(p), offset) will
// equal unalignedLoad32(p + offset).  Motivation: On x86-64 hardware we have
// empirically found that overlapping loads such as
//  unalignedLoad32(p) ... unalignedLoad32(p+1) ... unalignedLoad32(p+2)
// are slower than unalignedLoad64(p) followed by shifts and casts to uint32_t.
//
// We have different versions for 64- and 32-bit; ideally we would avoid the
// two functions and just inline the unalignedLoad64 call into
// getUint32AtOffset, but GCC (at least not as of 4.6) is seemingly not clever
// enough to avoid loading the value multiple times then. For 64-bit, the load
// is done when getEightBytesAt() is called, whereas for 32-bit, the load is
// done at getUint32AtOffset() time.

template<unsigned> class EightBytesReference;

template<> class EightBytesReference<8>
{
public:
	void setAddress(const char* ptr)
	{
		data = unalignedLoad64(ptr);
	}
	template<int OFFSET> uint32_t getUint32AtOffset() const
	{
		static_assert((OFFSET >= 0) && (OFFSET <= 4), "must be in [0..4]");
		int shift = OPENMSX_BIGENDIAN ? (32 - 8 * OFFSET)
		                              : (     8 * OFFSET);
		return data >> shift;
	}
private:
	uint64_t data;
};

template<> class EightBytesReference<4>
{
public:
	void setAddress(const char* ptr_)
	{
		ptr = ptr_;
	}
	template<int OFFSET> uint32_t getUint32AtOffset() const
	{
		static_assert((OFFSET >= 0) && (OFFSET <= 4), "must be in [0..4]");
		return unalignedLoad32(ptr + OFFSET);
	}
private:
	const char* ptr;
};



// Count the number of trailing zero bytes (=trailing zero bits divided by 8).
// Returns an undefined value if n == 0.
template<typename T> static inline unsigned ctzDiv8(T n)
{
	static const unsigned DIV = 8;
#if (defined(__i386__) || defined(__x86_64__)) && \
    ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR >= 3)))
	// Gcc-3.4 or above have the following builtins. Though we only want to
	// use them if they are very fast. E.g. on x86 they compile to a single
	// 'bsf' instruction. If they have to be emulated in software then the
	// fallback code below is likely faster (because it skips part of the
	// full ctz calculation).
	// TODO on vc++ we could use
	//   _BitScanForward and _BitScanForward64
	// and possibly other compilers have something similar.
	if (sizeof(T) <= 4) {
		return __builtin_ctz(n) / DIV;
	} else {
		return __builtin_ctzll(n) / DIV;
	}
#else
	// Classical ctz routine, but skip the last 3 (for DIV=8) iterations.
	unsigned bits = 8 * sizeof(T);
	unsigned r = (bits - 1) / DIV;
	for (unsigned shift = bits / 2; shift >= DIV; shift /= 2) {
		if (T x = n << shift) {
			n = x;
			r -= shift / DIV;
		}
	}
	return r;
#endif

}

// Return the largest n such that
//
//   s1[0,n-1] == s2[0,n-1]
//   and n <= (s2Limit - s2).
//
// Does not read *s2Limit or beyond.
// Does not read *(s1 + (s2Limit - s2)) or beyond.
// Requires that s2Limit >= s2.
//
// This implementation is tuned for 64-bit, little-endian machines. But it
// also works fine on 32-bit and/or big-endian machines.

// search either 4 or 8 bytes at-a-time
template<int> struct FindMatchUnit;
template<> struct FindMatchUnit<4> { using type = uint32_t; };
template<> struct FindMatchUnit<8> { using type = uint64_t; };

static inline int findMatchLength(const char* s1,
                                  const char* s2, const char* s2Limit)
{
	assert(s2Limit >= s2);
	int matched = 0;

	// Find out how long the match is. We loop over the data N bits at a
	// time until we find a N-bit block that doesn't match, Then (only on
	// little endian machines) we find the first non-matching bit and use
	// that to calculate the total length of the match.
	using T = FindMatchUnit<sizeof(void*)>::type;
	while (likely(s2 <= s2Limit - sizeof(T))) {
		T l2 = unalignedLoad<T>(s2);
		T l1 = unalignedLoad<T>(s1 + matched);
		if (unlikely(l2 == l1)) {
			s2      += sizeof(T);
			matched += sizeof(T);
		} else {
			if (OPENMSX_BIGENDIAN) break;
			// On current (mid-2008) Opteron models there is a 3%
			// more efficient code sequence to find the first
			// non-matching byte. However, what follows is ~10%
			// better on Intel Core 2 and newer, and we expect
			// AMD's bsf instruction to improve.
			return matched + ctzDiv8(l2 ^ l1);
		}
	}
	while ((s2 < s2Limit) && (s1[matched] == *s2)) {
		++s2;
		++matched;
	}
	return matched;
}


template<bool ALLOW_FAST_PATH>
static inline char* emitLiteral(char* op, const char* literal, int len)
{
	int n = len - 1; // Zero-length literals are disallowed
	if (n < 60) {
		// Fits in tag byte
		*op++ = LITERAL | (n << 2);

		// The vast majority of copies are below 16 bytes, for which a
		// call to memcpy is overkill. This fast path can sometimes
		// copy up to 15 bytes too much, but that is okay in the
		// main loop, since we have a bit to go on for both sides:
		//
		//   - The input will always have INPUT_MARGIN_BYTES = 15 extra
		//     available bytes, as long as we're in the main loop, and
		//     if not, ALLOW_FAST_PATH = false.
		//   - The output will always have 32 spare bytes (see
		//     maxCompressedLength()).
		if (ALLOW_FAST_PATH && len <= 16) {
			unalignedCopy128(literal, op);
			return op + len;
		}
	} else {
		// Encode in upcoming bytes
		char* base = op;
		op++;
		int count = 0;
		do {
			*op++ = n & 0xff;
			n >>= 8;
			count++;
		} while (n > 0);
		assert(count >= 1);
		assert(count <= 4);
		*base = LITERAL | ((59 + count) << 2);
	}
	memcpy(op, literal, len);
	return op + len;
}

static inline char* emitCopyLessThan64(char* op, size_t offset, int len)
{
	assert(len <= 64);
	assert(len >= 4);
	assert(offset < 65536);

	if ((len < 12) && (offset < 2048)) {
		size_t lenMinus4 = len - 4;
		assert(lenMinus4 < 8); // Must fit in 3 bits
		*op++ = char(COPY_1_BYTE_OFFSET + ((lenMinus4) << 2) + ((offset >> 8) << 5));
		*op++ = offset & 0xff;
	} else {
		*op++ = COPY_2_BYTE_OFFSET + ((len - 1) << 2);
		Endian::write_UA_L16(op, uint16_t(offset));
		op += 2;
	}
	return op;
}

static inline char* emitCopy(char* op, size_t offset, int len)
{
	// Emit 64 byte copies but make sure to keep at least four bytes reserved
	while (len >= 68) {
		op = emitCopyLessThan64(op, offset, 64);
		len -= 64;
	}

	// Emit an extra 60 byte copy if have too much data to fit in one copy
	if (len > 64) {
		op = emitCopyLessThan64(op, offset, 60);
		len -= 60;
	}

	// Emit remainder (at least 4 bytes)
	op = emitCopyLessThan64(op, offset, len);
	return op;
}

// The size of a compression block. Note that many parts of the compression
// code assumes that kBlockSize <= 65536; in particular, the hash table
// can only store 16-bit offsets, and EmitCopy() also assumes the offset
// is 65535 bytes or less.
static const size_t BLOCK_SIZE = 1 << 16;

// Compresses "input" string to the "*op" buffer.
//
// REQUIRES: "input" is at most "BLOCK_SIZE" bytes long.
// REQUIRES: "op" points to an array of memory that is at least
// "maxCompressedLength(input.size())" in size.
//
// Returns an "end" pointer into "op" buffer.
// "end - op" is the compressed size of "input".
static char* compressFragment(const char* input, size_t inputSize, char* op)
{
	static const int HASH_TABLE_BITS = 14;
	static const int SHIFT = 32 - HASH_TABLE_BITS;
	uint16_t table[1 << HASH_TABLE_BITS]; // 32KB
	memset(table, 0, sizeof(table));

	// "ip" is the input pointer, and "op" is the output pointer.
	const char* ip = input;
	const char* ipEnd = input + inputSize;
	assert(inputSize <= BLOCK_SIZE);
	// Bytes in [nextEmit, ip) will be emitted as literal bytes.  Or
	// [nextEmit, ipEnd) after the main loop.
	const char* nextEmit = ip;

	static const size_t INPUT_MARGIN_BYTES = 15;
	if (likely(inputSize >= INPUT_MARGIN_BYTES)) {
		const char* ipLimit = ipEnd - INPUT_MARGIN_BYTES;
		uint32_t nextHash = hash<SHIFT>(++ip);
		while (true) {
			assert(nextEmit < ip);
			// The body of this loop calls emitLiteral() once and
			// then emitCopy one or more times.  (The exception is
			// that when we're close to exhausting the input we
			// goto emitRemainder.)
			//
			// In the first iteration of this loop we're just
			// starting, so there's nothing to copy, so calling
			// emitLiteral() once is necessary.  And we only start
			// a new iteration when the current iteration has
			// determined that a call to emitLiteral() will precede
			// the next call to emitCopy (if any).
			//
			// Step 1: Scan forward in the input looking for a
			// 4-byte-long match. If we get close to exhausting the
			// input then goto emitRemainder.
			//
			// Heuristic match skipping: If 32 bytes are scanned
			// with no matches found, start looking only at every
			// other byte. If 32 more bytes are scanned, look at
			// every third byte, etc.. When a match is found,
			// immediately go back to looking at every byte. This
			// is a small loss (~5% performance, ~0.1% density) for
			// compressible data due to more bookkeeping, but for
			// non-compressible data (such as JPEG) it's a huge win
			// since the compressor quickly "realizes" the data is
			// incompressible and doesn't bother looking for
			// matches everywhere.
			//
			// The "skip" variable keeps track of how many bytes
			// there are since the last match; dividing it by 32
			// (ie. right-shifting by five) gives the number of
			// bytes to move ahead for each iteration.
			uint32_t skip = 32;
			const char* nextIp = ip;
			const char* candidate;
			do {
				ip = nextIp;
				uint32_t h = nextHash;
				assert(h == hash<SHIFT>(ip));
				uint32_t bytesBetweenHashLookups = skip++ >> 5;
				nextIp = ip + bytesBetweenHashLookups;
				if (unlikely(nextIp > ipLimit)) {
					goto emitRemainder;
				}
				nextHash = hash<SHIFT>(nextIp);
				candidate = input + table[h];
				assert(candidate >= input);
				assert(candidate < ip);

				table[h] = ip - input;
			} while (likely(unalignedLoad32(ip) !=
			                unalignedLoad32(candidate)));

			// Step 2: A 4-byte match has been found.  We'll later
			// see if more than 4 bytes match.  But, prior to the
			// match, input bytes [nextEmit, ip) are unmatched.
			// Emit them as "literal bytes."
			assert(nextEmit + 16 <= ipEnd);
			op = emitLiteral<true>(op, nextEmit, ip - nextEmit);

			// Step 3: Call emitCopy, and then see if another
			// emitCopy could be our next move.  Repeat until we
			// find no match for the input immediately after what
			// was consumed by the last emitCopy call.
			//
			// If we exit this loop normally then we need to call
			// emitLiteral() next, though we don't yet know how big
			// the literal will be.  We handle that by proceeding
			// to the next iteration of the main loop.  We also can
			// exit this loop via goto if we get close to
			// exhausting the input.
			EightBytesReference<sizeof(void*)> inputBytes;
			uint32_t candidateBytes = 0;
			do {
				// We have a 4-byte match at ip, and no need to
				// emit any "literal bytes" prior to ip.
				int matched = 4 + findMatchLength(candidate + 4, ip + 4, ipEnd);
				size_t offset = ip - candidate;
				assert(0 == memcmp(ip, candidate, matched));
				ip += matched;
				op = emitCopy(op, offset, matched);
				// We could immediately start working at ip
				// now, but to improve compression we first
				// update table[hash(ip - 1, ...)].
				const char* insertTail = ip - 1;
				nextEmit = ip;
				if (unlikely(ip >= ipLimit)) {
					goto emitRemainder;
				}
				inputBytes.setAddress(insertTail);
				uint32_t prevHash = hashBytes<SHIFT>(inputBytes.getUint32AtOffset<0>());
				table[prevHash] = ip - input - 1;
				uint32_t curHash = hashBytes<SHIFT>(inputBytes.getUint32AtOffset<1>());
				candidate = input + table[curHash];
				candidateBytes = unalignedLoad32(candidate);
				table[curHash] = ip - input;
			} while (inputBytes.getUint32AtOffset<1>() == candidateBytes);

			nextHash = hashBytes<SHIFT>(inputBytes.getUint32AtOffset<2>());
			++ip;
		}
	}

emitRemainder:
	// Emit the remaining bytes as a literal
	if (nextEmit < ipEnd) {
		op = emitLiteral<false>(op, nextEmit, ipEnd - nextEmit);
	}
	return op;
}

void compress(const char* input, size_t inLen,
              char* output, size_t& outLen)
{
	char* out = output;
	while (inLen > 0) {
		size_t numToRead = std::min(inLen, BLOCK_SIZE);
		out = compressFragment(input, numToRead, out);
		inLen -= numToRead;
		input += numToRead;
	}
	outLen = out - output + SCRATCH_SIZE;
}

size_t maxCompressedLength(size_t inLen)
{
	return 32 + SCRATCH_SIZE + inLen + inLen / 6;
}

}
