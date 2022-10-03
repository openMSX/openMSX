#ifndef ITERABLEBITSET_HH
#define ITERABLEBITSET_HH

#include "ranges.hh"
#include "xrange.hh"
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

/** IterableBitSet. A collection of N bits, with N a compile-time constant.
  *
  * The main difference with std::bitset is that this implementation allows to
  * efficiently:
  *  - Set a whole range of bits.
  *  - Iterate over all set bits.
  *
  * This implementation is well suited for bitsets of size up to hundreds or
  * even a few thousand bits. For much larger bitsets and/or for bitsets with
  * specific patterns (e.g. very sparse, or large bursts), other implementations
  * might be better suited.
  *
  * The interface of this class is (intentionally) minimal. If/when needed we
  * can easily extend it.
  */
template<size_t N>
class IterableBitSet
{
	using WordType = std::conditional_t<(N > 32), uint64_t,
	                 std::conditional_t<(N > 16), uint32_t,
	                 std::conditional_t<(N >  8), uint16_t,
	                                              uint8_t>>>;
	static constexpr size_t BITS_PER_WORD = 8 * sizeof(WordType);
	static constexpr size_t NUM_WORDS = (N + BITS_PER_WORD - 1) / BITS_PER_WORD;

public:
	/** (Implicit) default constructor. Sets all bits to '0'.
	  */

	/** Returns true iff none of the bits are set.
	  */
	[[nodiscard]] bool empty() const
	{
		return ranges::all_of(words, [](auto w) { return w == 0; });
	}

	/** Set the (single) bit at position 'pos' to '1'.
	  */
	void set(size_t pos)
	{
		assert(pos < N);
		if constexpr (NUM_WORDS > 1) {
			auto w = pos / BITS_PER_WORD;
			auto m = WordType(1) << (pos % BITS_PER_WORD);
			words[w] |= m;
		} else {
			words[0] |= WordType(1) << pos;
		}
	}

	/** Starting from position 'pos', set the 'n' following bits to '1'.
	  */
	void setPosN(size_t pos, size_t n)
	{
		setRange(pos, pos + n);
	}

	/** Set all bits in the half-open range [begin, end) to '1'.
	  */
	void setRange(size_t begin, size_t end)
	{
		assert(begin <= end);
		assert(end <= N);

		if (begin == end) return;

		if constexpr (NUM_WORDS > 1) {
			auto firstWord = begin / BITS_PER_WORD;
			auto lastWord = (end - 1) / BITS_PER_WORD;
			if (firstWord != lastWord) {
				auto firstPos = begin % BITS_PER_WORD;
				auto firstMask = WordType(-1) << firstPos;
				words[firstWord++] |= firstMask;

				while (firstWord != lastWord) {
					words[firstWord++] = WordType(-1);
				}

				auto lastPos = end % BITS_PER_WORD;
				auto lastMask = WordType(-1) >> (lastPos ? (BITS_PER_WORD - lastPos) : 0);
				words[firstWord] |= lastMask;
			} else {
				auto firstPos = begin % BITS_PER_WORD;
				auto mask = WordType(-1) << firstPos;
				auto lastPos = end % BITS_PER_WORD;
				if (lastPos) {
					auto lastMask = WordType(-1) >> (BITS_PER_WORD - lastPos);
					mask &= lastMask;
				}
				words[firstWord] |= mask;
			}
		} else {
			assert(end);
			auto mask1 = WordType(-1) << begin;
			auto mask2 = WordType(-1) >> (BITS_PER_WORD - end);
			words[0] |= mask1 & mask2;
		}
	}

	/** Execute the given operation 'op' for all '1' bits.
	  * The operation is called with the index of the bit as parameter.
	  * The bits are visited in ascending order.
	  */
	void foreachSetBit(std::invocable<size_t> auto op) const
	{
		for (auto i : xrange(NUM_WORDS)) {
			auto w = words[i];
			while (w) {
				op(i * BITS_PER_WORD + std::countr_zero(w));
				w &= w - 1; // clear least significant set bit
			}
		}
	}

private:
	std::array<WordType, NUM_WORDS> words = {};
};

#endif
