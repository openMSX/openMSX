#ifndef POWER_OF_TWO_HH
#define POWER_OF_TWO_HH

#include "narrow.hh"
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>

namespace openmsx {

/**
 * A constexpr power_of_two struct for power of two numbers, with compact
 * storage and built-in value correctness assertion. Additionally, the compiler
 * optimises multiplication / division / modulo operations to bit shifts.
 */
template<std::unsigned_integral T>
struct power_of_two {
	constexpr power_of_two(T size)
		: exponent(narrow_cast<uint8_t>(std::bit_width(size - 1)))
	{
		assert(std::has_single_bit(size));
	};

	[[nodiscard]] constexpr operator T() const
	{
		return T{1} << exponent;
	}

	uint8_t exponent = 0;
};

} // namespace openmsx

#endif
