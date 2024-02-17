#ifndef MINIMAL_PERFECT_HASH_HH
#define MINIMAL_PERFECT_HASH_HH

#include "cstd.hh"
#include "Math.hh"
#include "ranges.hh"
#include "static_vector.hh"
#include "stl.hh"
#include "xrange.hh"
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>

// This calculates an "order preserving minimal perfect hash function" (mph).
//
// That is for N given input keys (typically these keys will be strings) we
// produce a function with the following properties:
// - The function takes a key as input and produces a number.
// - For the i-th input key (i in [0..N)) the functions returns the value 'i'.
//   * Distinct inputs produce a distinct output (it's a 'perfect' hash).
//   * There are no gaps in the output range (the function is 'minimal').
//   * The order between input and output is preserved.
// - For a new input that was not part of the original set, this function still
//   produces a value in the range [0..n), but it doesn't give any guarantee
//   about which specific value it returns. So typically this output will be
//   used as an index in an array to lookup the original key and then do an
//   equality comparison on that (single) key.
// - The function runs in constant time (O(1)). (Comparing all keys one by one
//   would be O(N), using a std::map would be O(log(N))). More in detail, the
//   input key is hashed (with given non-perfect hash function) only once,
//   followed by one or sometimes two table-lookups.
// - The function can be calculated at compile-time (constexpr).
// - The required storage is about 2 bytes per input key.
//
// This implementation is based on
//    Frozen - An header-only, constexpr alternative to gperf for C++14 users
//    https://github.com/serge-sans-paille/frozen
// Which is based on Steve Hanov's blog post:
//    http://stevehanov.ca/blog/index.php?id=119
// Which is based on this paper:
//    Hash, displace, and compress
//    Djamal Belazzougui, Fabiano C. Botelho and Martin Dietzfelbinger
//    http://cmph.sourceforge.net/papers/esa09.pdf
//
// Compared to the original implementation in 'Frozen' this version is more
// stripped down. There is a chance that the calculation of the mph fails. That
// chance is a bit bigger in our version(*1). On the other hand our produced
// functions runs a bit faster (only runs the non-perfect hash function once
// instead of once or twice). And the resulting tables are ~10x smaller in
// memory (for the specific use case in openMSX with only about ~100 input
// keys).
// (*1) If the calculation fails, it fails at compile time. And for the current
// use case in openMSX (mapper type names) it works fine. If in the future we
// add many (+20) more mapper types we may need to tweak this code a little.

namespace PerfectMinimalHash {

template<size_t M, typename Hash>
struct Result
{
	static_assert(std::has_single_bit(M));
	static constexpr auto M2 = M;

	std::array<uint8_t, M > tab1;
	std::array<uint8_t, M2> tab2; // Note: half this size is not enough, tried that before
	[[no_unique_address]] Hash hash;

	[[nodiscard]] constexpr uint8_t lookupIndex(const auto& key) const {
		const uint32_t h = hash(key);
		const uint8_t d = tab1[h % M];
		if ((d & 0x80) == 0) {
			return d;
		} else {
			return tab2[(h >> (d & 31)) % M2];
		}
	}
};

template<size_t N, typename Hash, typename GetKey>
[[nodiscard]] constexpr auto create(const Hash& hash, const GetKey& getKey)
{
	static_assert(N < 128);
	constexpr size_t M = std::bit_ceil(N);
	constexpr auto bucket_max = size_t(2 * cstd::sqrt(M));

	Result<M, Hash> r{};
	r.hash = hash;

	// Step 1: Place all of the keys into buckets
	std::array<static_vector<uint8_t, bucket_max>, r.tab1.size()> buckets;
	for (auto i : xrange(N)) {
		buckets[hash(getKey(i)) % r.tab1.size()].push_back(uint8_t(i));
	}

	// Step 2: Sort the buckets to process the ones with the most items first.
	ranges::sort(buckets, [](const auto& x, const auto& y) {
		// sort largest first
		if (x.size() != y.size()) {
			return x.size() > y.size();
		}
		// same size, sort lexicographical
		return std::lexicographical_compare(x.begin(), x.end(),
		                                    y.begin(), y.end());
	});

	// Step 3: Map the items in buckets into hash tables.
	constexpr auto UNUSED = uint8_t(-1);
	ranges::fill(r.tab2, UNUSED);
	for (const auto& bucket : buckets) {
		auto const bSize = bucket.size();
		if (bSize == 0) break; // done
		auto hash1 = hash(getKey(bucket[0])) % r.tab1.size();
		if (bSize == 1) {
			// Store index to the (single) item in tab1
			r.tab1[hash1] = bucket[0];
		} else {
			// Repeatedly try different shift-factors until we can
			// place all items in the bucket into free slots.
			uint8_t shift = 1;
			static_vector<uint8_t, bucket_max> bucket_slots;

			while (bucket_slots.size() < bSize) {
				auto hash2 = hash(getKey(bucket[bucket_slots.size()]));
				assert((hash2 % r.tab1.size()) == hash1);
				auto slot = (hash2 >> shift) % r.tab2.size();
				if (r.tab2[slot] != UNUSED || contains(bucket_slots, slot)) {
					++shift; // try next
					assert(shift < 32);
					bucket_slots.clear();
					continue;
				}
				bucket_slots.push_back(uint8_t(slot));
			}

			// Put successful shift-factor in tab1, and put indices to items in their slots
			r.tab1[hash1] = shift | 0x80;
			for (auto i : xrange(bSize)) {
				r.tab2[bucket_slots[i]] = bucket[i];
			}
		}
	}

	// Change unused entries to zero because it must be valid indices (< N).
	ranges::replace(r.tab2, UNUSED, uint8_t(0));
	return r;
}

} // namespace PerfectMinimalHash

#endif
