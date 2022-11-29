#ifndef FIXEDPOINT_HH
#define FIXEDPOINT_HH

#include "narrow.hh"
#include "serialize.hh"
#include <cmath>
#include <cstdint>

namespace openmsx {

/** A fixed point number, implemented by a 32-bit signed integer.
  * The FRACTION_BITS template argument selects the position of the "binary
  * point" (base 2 equivalent to decimal point).
  */
template<unsigned FRACTION_BITS_>
class FixedPoint {
public:
	/** Number of fractional bits (export template parameter as a constant
	  * so that external code can use it more easily). */
	static constexpr unsigned FRACTION_BITS = FRACTION_BITS_;

private:
	/** Fixed point representation of 1.
	  */
	static constexpr int ONE = 1 << FRACTION_BITS;

	/** Precalculated float value of 1 / ONE.
	  */
	static constexpr float INV_ONE_F = 1.0f / ONE;

	/** Precalculated double value of 1 / ONE.
	  */
	static constexpr double INV_ONE_D = 1.0 / ONE;

	/** Bitmask to filter out the fractional part of a fixed point
	  * representation.
	  */
	static constexpr int FRACTION_MASK = ONE - 1;

public:
	/** Create new fixed point object from given representation.
	  * Used by the overloaded operators.
	  * @param value the internal representation.
	  */
	[[nodiscard]] static constexpr FixedPoint create(int value) {
		return FixedPoint(value, CreateRawTag{});
	}

	/** Creates a zero-initialized fixed point object.
	  */
	constexpr FixedPoint() = default;

	// Conversion to fixed point:

	explicit constexpr FixedPoint(int i)      : value(i << FRACTION_BITS) {}
	explicit constexpr FixedPoint(unsigned i) : value(i << FRACTION_BITS) {}
	explicit FixedPoint(float  f) : value(narrow_cast<int>(lrintf(f * ONE))) {}
	explicit FixedPoint(double d) : value(narrow_cast<int>(lrint (d * ONE))) {}

	[[nodiscard]] static constexpr FixedPoint roundRatioDown(unsigned n, unsigned d) {
		return create(narrow_cast<int>((static_cast<uint64_t>(n) << FRACTION_BITS) / d));
	}

	[[nodiscard]] static constexpr int shiftHelper(int x, int s) {
		return (s >= 0) ? (x >> s) : (x << -s);
	}
	template<unsigned BITS2>
	explicit constexpr FixedPoint(FixedPoint<BITS2> other)
		: value(shiftHelper(other.getRawValue(), BITS2 - FRACTION_BITS)) {}

	// Conversion from fixed point:

	/**
	 * Returns the integer part (rounded down) of this fixed point number.
	 * Note that for negative numbers, rounding occurs away from zero.
	 */
	[[nodiscard]] constexpr int toInt() const {
		return value >> FRACTION_BITS;
	}

	/**
	 * Returns the float value that corresponds to this fixed point number.
	 */
	[[nodiscard]] constexpr float toFloat() const {
		return narrow_cast<float>(value) * INV_ONE_F;
	}

	/**
	 * Returns the double value that corresponds to this fixed point number.
	 */
	[[nodiscard]] constexpr double toDouble() const {
		return narrow_cast<double>(value) * INV_ONE_D;
	}

	/**
	 * Returns the fractional part of this fixed point number as a float.
	 * The fractional part is never negative, even for negative fixed point
	 * numbers.
	 * x.toInt() + x.fractionAsFloat() is approximately equal to x.toFloat()
	 */
	[[nodiscard]] constexpr float fractionAsFloat() const {
		return narrow_cast<float>(value & FRACTION_MASK) * INV_ONE_F;
	}

	/**
	 * Returns the fractional part of this fixed point number as a double.
	 * The fractional part is never negative, even for negative fixed point
	 * numbers.
	 * x.toInt() + x.fractionAsDouble() is approximately equal to x.toDouble()
	 */
	[[nodiscard]] constexpr double fractionAsDouble() const {
		return narrow_cast<double>(value & FRACTION_MASK) * INV_ONE_D;
	}

	// Various arithmetic:

	/**
	 * Returns the result of a division between this fixed point number and
	 * another, rounded towards zero.
	 */
	[[nodiscard]] constexpr int divAsInt(FixedPoint other) const {
		return value / other.value;
	}

	/**
	 * Returns this value rounded down.
	 * The result is equal to FixedPoint(fp.toInt()).
	 */
	[[nodiscard]] constexpr FixedPoint floor() const {
		return create(value & ~FRACTION_MASK);
	}

	/**
	 * Returns the fractional part of this value.
	 * The result is equal to fp - floor(fp).
	 */
	[[nodiscard]] constexpr FixedPoint fract() const {
		return create(value & FRACTION_MASK);
	}

	/**
	 * Returns the fractional part of this value as an integer.
	 * The result is equal to  (fract() * (1 << FRACTION_BITS)).toInt()
	 */
	[[nodiscard]] constexpr unsigned fractAsInt() const {
		return value & FRACTION_MASK;
	}

	// Arithmetic operators:

	[[nodiscard]] constexpr friend FixedPoint operator+(FixedPoint x, FixedPoint y) {
		return create(x.value + y.value);
	}
	[[nodiscard]] constexpr friend FixedPoint operator-(FixedPoint x, FixedPoint y) {
		return create(x.value - y.value);
	}
	[[nodiscard]] constexpr friend FixedPoint operator*(FixedPoint x, FixedPoint y) {
		return create(int(
			(static_cast<int64_t>(x.value) * y.value) >> FRACTION_BITS));
	}
	[[nodiscard]] constexpr friend FixedPoint operator*(FixedPoint x, int y) {
		return create(x.value * y);
	}
	[[nodiscard]] constexpr friend FixedPoint operator*(int x, FixedPoint y) {
		return create(x * y.value);
	}
	/**
	 * Divides two fixed point numbers.
	 * The fractional part is rounded down.
	 */
	[[nodiscard]] constexpr friend FixedPoint operator/(FixedPoint x, FixedPoint y) {
		return create(int(
			(static_cast<int64_t>(x.value) << FRACTION_BITS) / y.value));
	}
	[[nodiscard]] constexpr friend FixedPoint operator/(FixedPoint x, int y) {
		return create(x.value / y);
	}
	[[nodiscard]] constexpr friend FixedPoint operator<<(FixedPoint x, int y) {
		return create(x.value << y);
	}
	[[nodiscard]] constexpr friend FixedPoint operator>>(FixedPoint x, int y) {
		return create(x.value >> y);
	}

	// Comparison operators:
	[[nodiscard]] constexpr bool operator== (const FixedPoint&) const = default;
	[[nodiscard]] constexpr auto operator<=>(const FixedPoint&) const = default;

	// Arithmetic operators that modify this object:

	constexpr void operator+=(FixedPoint other) {
		value += other.value;
	}
	constexpr void operator-=(FixedPoint other) {
		value -= other.value;
	}

	/** Increase this value with the smallest possible amount. Typically
	  * used to implement counters at the resolution of this datatype.
	  */
	constexpr void addQuantum() {
		value += 1;
	}

	// Should only be used by other instances of this class
	//  templatized friend declarations are not possible in c++
	[[nodiscard]] constexpr int getRawValue() const {
		return value;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("value", value);
	}

private:
	struct CreateRawTag {};
	constexpr FixedPoint(int raw_value, CreateRawTag)
		: value(raw_value) {}

	int value = 0;
};

} // namespace openmsx

#endif // FIXEDPOINT_HH
