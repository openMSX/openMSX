#ifndef FIXEDPOINT_HH
#define FIXEDPOINT_HH

#include <cmath>
#include <cstdint>

namespace openmsx {

/** A fixed point number, implemented by a 32-bit signed integer.
  * The FRACTION_BITS template argument selects the position of the "binary
  * point" (base 2 equivalent to decimal point).
  */
template <unsigned FRACTION_BITS_>
class FixedPoint {
public:
	/** Number of fractional bits (export template parameter as a constant
	  * so that external code can use it more easily). */
	static const unsigned FRACTION_BITS = FRACTION_BITS_;

private:
	/** Fixed point representation of 1.
	  */
	static const int ONE = 1 << FRACTION_BITS;

	/** Precalculated float value of 1 / ONE.
	  */
	static const float INV_ONE_F;

	/** Precalculated double value of 1 / ONE.
	  */
	static const double INV_ONE_D;

	/** Bitmask to filter out the fractional part of a fixed point
	  * representation.
	  */
	static const int FRACTION_MASK = ONE - 1;

public:
	/** Create new fixed point object from given representation.
	  * Used by the overloaded operators.
	  * @param value the internal representation.
	  */
	static inline FixedPoint create(const int value) {
		FixedPoint ret;
		ret.value = value;
		return ret;
	}

	/** Creates an uninitialized fixed point object.
	  * This must be public to allow arrays of FixedPoint objects.
	  */
	explicit FixedPoint() {}

	// Conversion to fixed point:

	explicit FixedPoint(const int i) : value(i << FRACTION_BITS) {}
	explicit FixedPoint(const unsigned i) : value(i << FRACTION_BITS) {}
	explicit FixedPoint(const float f) : value(lrintf(f * ONE)) {}
	explicit FixedPoint(const double d) : value(lrint(d * ONE)) {}

	static FixedPoint roundRatioDown(unsigned n, unsigned d) {
		return create((static_cast<uint64_t>(n) << FRACTION_BITS) / d);
	}

	static inline int shiftHelper(int x, int s) {
		return (s >= 0) ? (x >> s) : (x << -s);
	}
	template <unsigned BITS2>
	explicit FixedPoint(FixedPoint<BITS2> other)
		: value(shiftHelper(other.getRawValue(), BITS2 - FRACTION_BITS)) {}

	// Conversion from fixed point:

	/**
	 * Returns the integer part (rounded down) of this fixed point number.
	 * Note that for negative numbers, rounding occurs away from zero.
	 */
	int toInt() const {
		return value >> FRACTION_BITS;
	}

	/**
	 * Returns the float value that corresponds to this fixed point number.
	 */
	float toFloat() const {
		return value * INV_ONE_F;
	}

	/**
	 * Returns the double value that corresponds to this fixed point number.
	 */
	double toDouble() const {
		return value * INV_ONE_D;
	}

	/**
	 * Returns the fractional part of this fixed point number as a float.
	 * The fractional part is never negative, even for negative fixed point
	 * numbers.
	 * x.toInt() + x.fractionAsFloat() is approximately equal to x.toFloat()
	 */
	float fractionAsFloat() const {
		return (value & FRACTION_MASK) * INV_ONE_F;
	}

	/**
	 * Returns the fractional part of this fixed point number as a double.
	 * The fractional part is never negative, even for negative fixed point
	 * numbers.
	 * x.toInt() + x.fractionAsDouble() is approximately equal to x.toDouble()
	 */
	double fractionAsDouble() const {
		return (value & FRACTION_MASK) * INV_ONE_D;
	}

	// Various arithmetic:

	/**
	 * Returns the result of a division between this fixed point number and
	 * another, rounded towards zero.
	 */
	int divAsInt(const FixedPoint other) const {
		return value / other.value;
	}

	/**
	 * Returns this value rounded down.
	 * The result is equal to FixedPoint(fp.toInt()).
	 */
	FixedPoint floor() const {
		return create(value & ~FRACTION_MASK);
	}

	/**
	 * Returns the fractional part of this value.
	 * The result is equal to fp - floor(fp).
	 */
	FixedPoint fract() const {
		return create(value & FRACTION_MASK);
	}

	/**
	 * Returns the fractional part of this value as an integer.
	 * The result is equal to  (fract() * (1 << FRACTION_BITS)).toInt()
	 */
	unsigned fractAsInt() const {
		return value & FRACTION_MASK;
	}

	// Arithmetic operators:

	FixedPoint operator+(const FixedPoint other) const {
		return create(value + other.value);
	}
	FixedPoint operator-(const FixedPoint other) const {
		return create(value - other.value);
	}
	FixedPoint operator*(const FixedPoint other) const {
		return create(int(
			(static_cast<int64_t>(value) * other.value) >> FRACTION_BITS));
	}
	FixedPoint operator*(const int i) const {
		return create(value * i);
	}
	/**
	 * Divides two fixed point numbers.
	 * The fractional part is rounded down.
	 */
	FixedPoint operator/(const FixedPoint other) const {
		return create(int(
			(static_cast<int64_t>(value) << FRACTION_BITS) / other.value));
	}
	FixedPoint operator/(const int i) const {
		return create(value / i);
	}
	FixedPoint operator<<(const int b) const {
		return create(value << b);
	}
	FixedPoint operator>>(const int b) const {
		return create(value >> b);
	}

	// Comparison operators:

	bool operator==(const FixedPoint other) const {
		return value == other.value;
	}
	bool operator!=(const FixedPoint other) const {
		return value != other.value;
	}
	bool operator<(const FixedPoint other) const {
		return value < other.value;
	}
	bool operator<=(const FixedPoint other) const {
		return value <= other.value;
	}
	bool operator>(const FixedPoint other) const {
		return value > other.value;
	}
	bool operator>=(const FixedPoint other) const {
		return value >= other.value;
	}

	// Arithmetic operators that modify this object:

	void operator+=(const FixedPoint other) {
		value += other.value;
	}
	void operator-=(const FixedPoint other) {
		value -= other.value;
	}

	/** Increase this value with the smallest possible amount. Typically
	  * used to implement counters at the resolution of this datatype.
	  */
	void addQuantum() {
		value += 1;
	}

	// Should only be used by other instances of this class
	//  templatized friend declarations are not possible in c++
	int getRawValue() const {
		return value;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("value", value);
	}

private:
	int value;
};

// Force all constants being defined, some compilers need this:

template <unsigned FRACTION_BITS>
const int FixedPoint<FRACTION_BITS>::ONE;

template <unsigned FRACTION_BITS>
const float FixedPoint<FRACTION_BITS>::INV_ONE_F = 1.0f / ONE;

template <unsigned FRACTION_BITS>
const double FixedPoint<FRACTION_BITS>::INV_ONE_D = 1.0 / ONE;

template <unsigned FRACTION_BITS>
const int FixedPoint<FRACTION_BITS>::FRACTION_MASK;

} // namespace openmsx

#endif // FIXEDPOINT_HH
