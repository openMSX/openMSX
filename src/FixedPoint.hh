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
	static inline FixedPoint<FRACTION_BITS> create(const int value) {
		FixedPoint<FRACTION_BITS> ret;
		ret.value = value;
		return ret;
	}
public:
	FixedPoint(const FixedPoint<FRACTION_BITS>& other) : value(other.value) {};
	FixedPoint(const int i) : value(i << FRACTION_BITS) {};
	FixedPoint(const float f) : value(lrintf(f * ONE)) {};
	FixedPoint(const double d) : value(lrint(d * ONE)) {};

	int toInt() const { return value >> FRACTION_BITS; }
	float toFloat() const { return static_cast<float>(value) / ONE; }
	double toDouble() const { return static_cast<double>(value) / ONE; }
	double fractionAsFloat() const {
		return static_cast<float>(value & FRACTION_MASK) / ONE;
	}
	double fractionAsDouble() const {
		return static_cast<double>(value & FRACTION_MASK) / ONE;
	}

	// Arithmetic operators:

	FixedPoint<FRACTION_BITS> operator+(
			const FixedPoint<FRACTION_BITS>& other) const {
		return create(value + other.value);
	}
	FixedPoint<FRACTION_BITS> operator-(
			const FixedPoint<FRACTION_BITS>& other) const {
		return create(value - other.value);
	}
	FixedPoint<FRACTION_BITS> operator*(
			const FixedPoint<FRACTION_BITS>& other) const {
		return create(static_cast<int>(
			(static_cast<long long int>(value) * other.value) >> FRACTION_BITS
			));
	}
	FixedPoint<FRACTION_BITS> operator*(const int i) const {
		return create(value * i);
	}
	FixedPoint<FRACTION_BITS> operator/(
			const FixedPoint<FRACTION_BITS>& other) const {
		return create(static_cast<int>(
			(static_cast<long long int>(value) << FRACTION_BITS) / other.value
			));
	}
	FixedPoint<FRACTION_BITS> operator/(const int i) const {
		return create(value / i);
	}

	// Comparison operators:

	bool operator==(const FixedPoint<FRACTION_BITS>& other) const {
		return value == other.value;
	}
	bool operator!=(const FixedPoint<FRACTION_BITS>& other) const {
		return value != other.value;
	}
	bool operator<(const FixedPoint<FRACTION_BITS>& other) const {
		return value < other.value;
	}
	bool operator<=(const FixedPoint<FRACTION_BITS>& other) const {
		return value <= other.value;
	}
	bool operator>(const FixedPoint<FRACTION_BITS>& other) const {
		return value > other.value;
	}
	bool operator>=(const FixedPoint<FRACTION_BITS>& other) const {
		return value >= other.value;
	}

	// Arithmetic operators that modify this object:

	void operator+=(const FixedPoint<FRACTION_BITS>& other) {
		value += other.value;
	}
	void operator-=(const FixedPoint<FRACTION_BITS>& other) {
		value -= other.value;
	}

private:
	int value;
};

} // namespace openmsx

#endif // FIXEDPOINT_HH
