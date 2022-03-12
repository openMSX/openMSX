/*
   This code is a based on xxhash, the main modifications are:
    - allow to produce case-insensitive (for ascii) hash values
    - tweak interface to fit openMSX (e.g. directly work on string_view)

- - - - - - - - - - - - - - - - - - - -

 original header:

   xxHash - Fast Hash algorithm
   Header File
   Copyright (C) 2012-2014, Yann Collet.
   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - xxHash source repository : http://code.google.com/p/xxhash/
*/

#ifndef XXHASH_HH
#define XXHASH_HH

#include "likely.hh"
#include <bit>
#include <cstdint>
#include <cstring>
#include <string_view>

constexpr uint32_t PRIME32_1 = 2654435761;
constexpr uint32_t PRIME32_2 = 2246822519;
constexpr uint32_t PRIME32_3 = 3266489917;
constexpr uint32_t PRIME32_4 =  668265263;
constexpr uint32_t PRIME32_5 =  374761393;


template<bool ALIGNED>
[[nodiscard]] inline uint32_t read32(const uint8_t* ptr)
{
	if constexpr (ALIGNED) {
#ifdef DEBUG
		assert((reinterpret_cast<intptr_t>(ptr) & 3) == 0);
#endif
		return *reinterpret_cast<const uint32_t*>(ptr);
	} else {
		uint32_t result;
		memcpy(&result, ptr, sizeof(result));
		return result;
	}
}


template<bool ALIGNED, uint8_t MASK8 = 0xFF, uint32_t SEED = 0>
[[nodiscard]] inline uint32_t xxhash_impl(const uint8_t* p, size_t size)
{
	constexpr uint32_t MASK32 = MASK8 * 0x01010101U;

	const uint8_t* const bEnd = p + size;
	uint32_t h32;

	if (unlikely(size >= 16)) {
		const uint8_t* const limit = bEnd - 16;

		// casts to avoid: warning C4307: '+': integral constant overflow
		uint32_t v1 = uint32_t(SEED + PRIME32_1 + uint64_t(PRIME32_2));
		uint32_t v2 = SEED + PRIME32_2;
		uint32_t v3 = SEED + 0;
		uint32_t v4 = SEED - PRIME32_1;

		do {
			uint32_t r1 = (read32<ALIGNED>(p +  0) & MASK32) * PRIME32_2;
			uint32_t r2 = (read32<ALIGNED>(p +  4) & MASK32) * PRIME32_2;
			uint32_t r3 = (read32<ALIGNED>(p +  8) & MASK32) * PRIME32_2;
			uint32_t r4 = (read32<ALIGNED>(p + 12) & MASK32) * PRIME32_2;
			p += 16;
			v1 = std::rotl(v1 + r1, 13) * PRIME32_1;
			v2 = std::rotl(v2 + r2, 13) * PRIME32_1;
			v3 = std::rotl(v3 + r3, 13) * PRIME32_1;
			v4 = std::rotl(v4 + r4, 13) * PRIME32_1;
		} while (p <= limit);

		h32 = std::rotl(v1, 1) + std::rotl(v2, 7) + std::rotl(v3, 12) + std::rotl(v4, 18);
	} else {
		h32  = SEED + PRIME32_5;
	}

	h32 += uint32_t(size);

	while ((p + 4) <= bEnd) {
		uint32_t r = (read32<ALIGNED>(p) & MASK32) * PRIME32_3;
		h32  = std::rotl(h32 + r, 17) * PRIME32_4;
		p += 4;
	}

	while (p < bEnd) {
		uint32_t r = (*p & MASK8) * PRIME32_5;
		h32 = std::rotl(h32 + r, 11) * PRIME32_1;
		p += 1;
	}

	h32  = (h32 ^ (h32 >> 15)) * PRIME32_2;
	h32  = (h32 ^ (h32 >> 13)) * PRIME32_3;
	return  h32 ^ (h32 >> 16);
}

template<uint8_t MASK8> [[nodiscard]] inline uint32_t xxhash_impl(std::string_view key)
{
	const auto* data = reinterpret_cast<const uint8_t*>(key.data());
	auto  size = key.size();
	if (reinterpret_cast<intptr_t>(data) & 3) {
		return xxhash_impl<false, MASK8>(data, size);
	} else {
		// Input is aligned, let's leverage the speed advantage
		return xxhash_impl<true,  MASK8>(data, size);
	}
}

[[nodiscard]] inline uint32_t xxhash(std::string_view key)
{
	return xxhash_impl<0xFF>(key);
}
[[nodiscard]] inline uint32_t xxhash_case(std::string_view key)
{
	return xxhash_impl<static_cast<uint8_t>(~('a' - 'A'))>(key);
}

struct XXHasher {
	[[nodiscard]] uint32_t operator()(std::string_view key) const {
		return xxhash(key);
	}
};

struct XXHasher_IgnoreCase {
	[[nodiscard]] uint32_t operator()(std::string_view key) const {
		return xxhash_case(key);
	}
};

#endif
