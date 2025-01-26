/*
Based on:
100% free public domain implementation of the SHA-1 algorithm
by Dominik Reichl <Dominik.Reichl@tiscali.de>

Refactored in C++ style as part of openMSX
by Maarten ter Huurne and Wouter Vermaelen.

=== Test Vectors (from FIPS PUB 180-1) ===

"abc"
A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D

"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1

A million repetitions of "a"
34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

#include "sha1.hh"

#include "MSXException.hh"

#include "endian.hh"
#include "narrow.hh"
#include "ranges.hh"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstring>
#ifdef __SSE2__
#include <emmintrin.h> // SSE2
#endif

namespace openmsx {

// Rotate x bits to the left
[[nodiscard]] static constexpr uint32_t rol32(uint32_t value, int bits)
{
	return (value << bits) | (value >> (32 - bits));
}

class WorkspaceBlock {
private:
	std::array<uint32_t, 16> data;

	[[nodiscard]] uint32_t next0(int i)
	{
		data[i] = Endian::readB32(&data[i]);
		return data[i];
	}
	[[nodiscard]] uint32_t next(int i)
	{
		return data[i & 15] = rol32(
			data[(i + 13) & 15] ^ data[(i + 8) & 15] ^
			data[(i +  2) & 15] ^ data[ i      & 15]
			, 1);
	}

public:
	explicit WorkspaceBlock(std::span<const uint8_t, 64> buffer);

	// SHA-1 rounds
	void r0(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += ((w & (x ^ y)) ^ y) + next0(i) + 0x5A827999 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r1(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += ((w & (x ^ y)) ^ y) + next(i) + 0x5A827999 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r2(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (w ^ x ^ y) + next(i) + 0x6ED9EBA1 + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r3(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (((w | x) & y) | (w & x)) + next(i) + 0x8F1BBCDC + rol32(v, 5);
		w = rol32(w, 30);
	}
	void r4(uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, int i)
	{
		z += (w ^ x ^ y) + next(i) + 0xCA62C1D6 + rol32(v, 5);
		w = rol32(w, 30);
	}
};

WorkspaceBlock::WorkspaceBlock(std::span<const uint8_t, 64> buffer)
{
	memcpy(data.data(), buffer.data(), sizeof(data));
}


// class Sha1Sum

Sha1Sum::Sha1Sum()
{
	clear();
}

Sha1Sum::Sha1Sum(std::string_view hex)
{
	if (hex.size() != 40) {
		throw MSXException("Invalid sha1, should be exactly 40 digits long: ", hex);
	}
	parse40(subspan<40>(hex));
}

#ifdef __SSE2__
// emulate some missing unsigned-8-bit comparison functions
[[nodiscard]] static inline __m128i _mm_cmpge_epu8(__m128i a, __m128i b)
{
	return _mm_cmpeq_epi8(_mm_max_epu8(a, b), a);
}

[[nodiscard]] static inline __m128i _mm_cmple_epu8(__m128i a, __m128i b)
{
	return _mm_cmpge_epu8(b, a);
}

// load 64-bit (possibly unaligned) and swap bytes
[[nodiscard]] static inline int64_t loadSwap64(const char* s)
{
	return narrow_cast<int64_t>(Endian::read_UA_B64(s));
}

#else

[[nodiscard]] static /*constexpr*/ unsigned hex(char x, const char* str)
{
	if (('0' <= x) && (x <= '9')) return x - '0';
	if (('a' <= x) && (x <= 'f')) return x - 'a' + 10;
	if (('A' <= x) && (x <= 'F')) return x - 'A' + 10;
	throw MSXException("Invalid sha1, digits should be 0-9, a-f: ",
	                   std::string_view(str, 40));
}
#endif

void Sha1Sum::parse40(std::span<const char, 40> str)
{
#ifdef __SSE2__
	// SSE2 version

	// load characters
	__m128i s0 = _mm_set_epi64x(loadSwap64(&str[ 8]),     loadSwap64(&str[ 0]));
	__m128i s1 = _mm_set_epi64x(loadSwap64(&str[24]),     loadSwap64(&str[16]));
	__m128i s2 = _mm_set_epi64x('0' * 0x0101010101010101, loadSwap64(&str[32]));

	// chars - '0'
	__m128i cc0 = _mm_set1_epi8(char(-'0'));
	__m128i s0_0 = _mm_add_epi8(s0, cc0);
	__m128i s1_0 = _mm_add_epi8(s1, cc0);
	__m128i s2_0 = _mm_add_epi8(s2, cc0);

	// (chars | 32) - 'a'  (convert uppercase 'A'-'F' into lower case)
	__m128i c32 = _mm_set1_epi8(32);
	__m128i cca = _mm_set1_epi8(char(-'a'));
	__m128i s0_a = _mm_add_epi8(_mm_or_si128(s0, c32), cca);
	__m128i s1_a = _mm_add_epi8(_mm_or_si128(s1, c32), cca);
	__m128i s2_a = _mm_add_epi8(_mm_or_si128(s2, c32), cca);

	// was in range '0'-'9'?
	__m128i c9  = _mm_set1_epi8(9);
	__m128i c0_0 = _mm_cmple_epu8(s0_0, c9);
	__m128i c1_0 = _mm_cmple_epu8(s1_0, c9);
	__m128i c2_0 = _mm_cmple_epu8(s2_0, c9);

	// was in range 'a'-'f'?
	__m128i c5  = _mm_set1_epi8(5);
	__m128i c0_a = _mm_cmple_epu8(s0_a, c5);
	__m128i c1_a = _mm_cmple_epu8(s1_a, c5);
	__m128i c2_a = _mm_cmple_epu8(s2_a, c5);

	// either '0'-'9' or 'a'-f' must be in range for all chars
	__m128i ok0 = _mm_or_si128(c0_0, c0_a);
	__m128i ok1 = _mm_or_si128(c1_0, c1_a);
	__m128i ok2 = _mm_or_si128(c2_0, c2_a);
	__m128i ok = _mm_and_si128(_mm_and_si128(ok0, ok1), ok2);
	if (_mm_movemask_epi8(ok) != 0xffff) [[unlikely]] {
		throw MSXException("Invalid sha1, digits should be 0-9, a-f: ",
		                   std::string_view(str.data(), 40));
	}

	// '0'-'9' to numeric value (or zero)
	__m128i d0_0 = _mm_and_si128(s0_0, c0_0);
	__m128i d1_0 = _mm_and_si128(s1_0, c1_0);
	__m128i d2_0 = _mm_and_si128(s2_0, c2_0);

	// 'a'-'f' to numeric value (or zero)
	__m128i c10 = _mm_set1_epi8(10);
	__m128i d0_a = _mm_and_si128(_mm_add_epi8(s0_a, c10), c0_a);
	__m128i d1_a = _mm_and_si128(_mm_add_epi8(s1_a, c10), c1_a);
	__m128i d2_a = _mm_and_si128(_mm_add_epi8(s2_a, c10), c2_a);

	// combine  0-9 / 10-15  into  0-15
	__m128i d0 = _mm_or_si128(d0_0, d0_a);
	__m128i d1 = _mm_or_si128(d1_0, d1_a);
	__m128i d2 = _mm_or_si128(d2_0, d2_a);

	// compact bytes to nibbles
	__m128i c00ff = _mm_set1_epi16(0x00ff);
	__m128i e0 = _mm_and_si128(_mm_or_si128(d0, _mm_srli_epi16(d0, 4)), c00ff);
	__m128i e1 = _mm_and_si128(_mm_or_si128(d1, _mm_srli_epi16(d1, 4)), c00ff);
	__m128i e2 = _mm_and_si128(_mm_or_si128(d2, _mm_srli_epi16(d2, 4)), c00ff);
	__m128i f0 = _mm_packus_epi16(e0, e0);
	__m128i f1 = _mm_packus_epi16(e1, e1);
	__m128i f2 = _mm_packus_epi16(e2, e2);

	// store result
	_mm_storeu_si128(std::bit_cast<__m128i*>(a.data()), _mm_unpacklo_epi64(f0, f1));
	a[4] = _mm_cvtsi128_si32(f2);
#else
	// equivalent c++ version
	const char* p = str.data();
	for (auto& ai : a) {
		unsigned t = 0;
		repeat(8, [&] {
			t <<= 4;
			t |= hex(*p++, str.data());
		});
		ai = t;
	}
#endif
}

[[nodiscard]] static constexpr char digit(unsigned x)
{
	return narrow<char>((x < 10) ? (x + '0') : (x - 10 + 'a'));
}
std::string Sha1Sum::toString() const
{
	std::array<char, 40> buf;
	size_t i = 0;
	for (const auto& ai : a) {
		for (int j = 28; j >= 0; j -= 4) {
			buf[i++] = digit((ai >> j) & 0xf);
		}
	}
	return {buf.data(), buf.size()};
}

bool Sha1Sum::empty() const
{
	return std::ranges::all_of(a, [](auto& e) { return e == 0; });
}
void Sha1Sum::clear()
{
	std::ranges::fill(a, 0);
}


// class SHA1

SHA1::SHA1()
{
	// SHA1 initialization constants
	m_state.a[0] = 0x67452301;
	m_state.a[1] = 0xEFCDAB89;
	m_state.a[2] = 0x98BADCFE;
	m_state.a[3] = 0x10325476;
	m_state.a[4] = 0xC3D2E1F0;
}

void SHA1::transform(std::span<const uint8_t, 64> buffer)
{
	WorkspaceBlock block(buffer);

	// Copy m_state[] to working vars
	uint32_t a = m_state.a[0];
	uint32_t b = m_state.a[1];
	uint32_t c = m_state.a[2];
	uint32_t d = m_state.a[3];
	uint32_t e = m_state.a[4];

	// 4 rounds of 20 operations each. Loop unrolled
	block.r0(a,b,c,d,e, 0); block.r0(e,a,b,c,d, 1); block.r0(d,e,a,b,c, 2);
	block.r0(c,d,e,a,b, 3); block.r0(b,c,d,e,a, 4); block.r0(a,b,c,d,e, 5);
	block.r0(e,a,b,c,d, 6); block.r0(d,e,a,b,c, 7); block.r0(c,d,e,a,b, 8);
	block.r0(b,c,d,e,a, 9); block.r0(a,b,c,d,e,10); block.r0(e,a,b,c,d,11);
	block.r0(d,e,a,b,c,12); block.r0(c,d,e,a,b,13); block.r0(b,c,d,e,a,14);
	block.r0(a,b,c,d,e,15); block.r1(e,a,b,c,d,16); block.r1(d,e,a,b,c,17);
	block.r1(c,d,e,a,b,18); block.r1(b,c,d,e,a,19); block.r2(a,b,c,d,e,20);
	block.r2(e,a,b,c,d,21); block.r2(d,e,a,b,c,22); block.r2(c,d,e,a,b,23);
	block.r2(b,c,d,e,a,24); block.r2(a,b,c,d,e,25); block.r2(e,a,b,c,d,26);
	block.r2(d,e,a,b,c,27); block.r2(c,d,e,a,b,28); block.r2(b,c,d,e,a,29);
	block.r2(a,b,c,d,e,30); block.r2(e,a,b,c,d,31); block.r2(d,e,a,b,c,32);
	block.r2(c,d,e,a,b,33); block.r2(b,c,d,e,a,34); block.r2(a,b,c,d,e,35);
	block.r2(e,a,b,c,d,36); block.r2(d,e,a,b,c,37); block.r2(c,d,e,a,b,38);
	block.r2(b,c,d,e,a,39); block.r3(a,b,c,d,e,40); block.r3(e,a,b,c,d,41);
	block.r3(d,e,a,b,c,42); block.r3(c,d,e,a,b,43); block.r3(b,c,d,e,a,44);
	block.r3(a,b,c,d,e,45); block.r3(e,a,b,c,d,46); block.r3(d,e,a,b,c,47);
	block.r3(c,d,e,a,b,48); block.r3(b,c,d,e,a,49); block.r3(a,b,c,d,e,50);
	block.r3(e,a,b,c,d,51); block.r3(d,e,a,b,c,52); block.r3(c,d,e,a,b,53);
	block.r3(b,c,d,e,a,54); block.r3(a,b,c,d,e,55); block.r3(e,a,b,c,d,56);
	block.r3(d,e,a,b,c,57); block.r3(c,d,e,a,b,58); block.r3(b,c,d,e,a,59);
	block.r4(a,b,c,d,e,60); block.r4(e,a,b,c,d,61); block.r4(d,e,a,b,c,62);
	block.r4(c,d,e,a,b,63); block.r4(b,c,d,e,a,64); block.r4(a,b,c,d,e,65);
	block.r4(e,a,b,c,d,66); block.r4(d,e,a,b,c,67); block.r4(c,d,e,a,b,68);
	block.r4(b,c,d,e,a,69); block.r4(a,b,c,d,e,70); block.r4(e,a,b,c,d,71);
	block.r4(d,e,a,b,c,72); block.r4(c,d,e,a,b,73); block.r4(b,c,d,e,a,74);
	block.r4(a,b,c,d,e,75); block.r4(e,a,b,c,d,76); block.r4(d,e,a,b,c,77);
	block.r4(c,d,e,a,b,78); block.r4(b,c,d,e,a,79);

	// Add the working vars back into m_state[]
	m_state.a[0] += a;
	m_state.a[1] += b;
	m_state.a[2] += c;
	m_state.a[3] += d;
	m_state.a[4] += e;
}

// Use this function to hash in binary data and strings
void SHA1::update(std::span<const uint8_t> data)
{
	assert(!m_finalized);
	uint32_t j = m_count & 63;

	size_t len = data.size();
	m_count += len;

	size_t i;
	if ((j + len) > 63) {
		i = 64 - j;
		ranges::copy(data.subspan(0, i), subspan(m_buffer, j));
		transform(m_buffer);
		for (/**/; i + 63 < len; i += 64) {
			transform(subspan<64>(data, i));
		}
		j = 0;
	} else {
		i = 0;
	}
	ranges::copy(data.subspan(i, len - i), subspan(m_buffer, j));
}

void SHA1::finalize()
{
	assert(!m_finalized);

	uint32_t j = m_count & 63;
	m_buffer[j++] = 0x80;
	if (j > 56) {
		std::ranges::fill(subspan(m_buffer, j, 64 - j), 0);
		transform(m_buffer);
		j = 0;
	}
	std::ranges::fill(subspan(m_buffer, j, 56 - j), 0);
	Endian::B64 finalCount(8 * m_count); // convert number of bytes to bits
	memcpy(&m_buffer[56], &finalCount, 8);
	transform(m_buffer);

	m_finalized = true;
}

Sha1Sum SHA1::digest()
{
	if (!m_finalized) finalize();
	return m_state;
}

Sha1Sum SHA1::calc(std::span<const uint8_t> data)
{
	SHA1 sha1;
	sha1.update(data);
	return sha1.digest();
}

} // namespace openmsx
