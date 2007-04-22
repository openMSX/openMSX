// $Id$

#ifndef FIXEDPOINT_HH
#define FIXEDPOINT_HH

#include <cmath>

namespace openmsx {

/** A fixed point number, implemented by a 32-bit signed integer.
  * The FRACTION_BITS template argument selects the position of the decimal
  * dot.
  */
template <unsigned FRACTION_BITS>
class FixedPoint {
private:
	/** Fixed point representation of 1.
	  */
	static const int ONE = 1 << FRACTION_BITS;

	/** Precalculated float value of 1 / ONE.
	  */
	static const float INV_ONE_F = 1.0f / ONE;

	/** Precalculated double value of 1 / ONE.
	  */
	static const double INV_ONE_D = 1.0f / ONE;

	/** Bitmask to filter out the fractional part of a fixed point
	  * representation.
	  */
	static const int FRACTION_MASK = ONE - 1;

	/** Creates an uninitialized fixed point object. Used by create().
	  */
	FixedPoint() {};

	/** Create new floating point object from given representation.
	  * Used by the overloaded operators.
	  */
	static inline FixedPoint create(const int value) {
		FixedPoint ret;
		ret.value = value;
		return ret;
	}

public:
	explicit FixedPoint(const int i) : value(i << FRACTION_BITS) {};
	explicit FixedPoint(const float f) : value(lrintf(f * ONE)) {};
	explicit FixedPoint(const double d) : value(lrint(d * ONE)) {};

	int toInt() const { return value >> FRACTION_BITS; }
	float toFloat() const { return value * INV_ONE_F; }
	double toDouble() const { return value * INV_ONE_D; }
	float fractionAsFloat() const {
		return (value & FRACTION_MASK) * INV_ONE_F;
	}
	double fractionAsDouble() const {
		return (value & FRACTION_MASK) * INV_ONE_D;
	}

	// Arithmetic operators:

	FixedPoint operator+(const FixedPoint other) const {
		return create(value + other.value);
	}
	FixedPoint operator-(const FixedPoint other) const {
		return create(value - other.value);
	}
	FixedPoint operator*(const FixedPoint other) const {
		return create(static_cast<int>(
			(static_cast<long long int>(value) * other.value) >> FRACTION_BITS
			));
	}
	FixedPoint operator*(const int i) const {
		return create(value * i);
	}
	FixedPoint operator/(const FixedPoint other) const {
		return create(static_cast<int>(
			(static_cast<long long int>(value) << FRACTION_BITS) / other.value
			));
	}
	FixedPoint operator/(const int i) const {
		return create(value / i);
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

private:
	int value;
};

} // namespace openmsx

#endif // FIXEDPOINT_HH
