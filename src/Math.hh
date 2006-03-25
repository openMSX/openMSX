// $Id$

#ifndef MATH_HH
#define MATH_HH

#include <algorithm>
#include <cmath>

namespace openmsx {

namespace Math {

/** Returns the smallest number that is both >=a and a power of two.
  */
unsigned powerOfTwo(unsigned a);

void gaussian2(double& r1, double& r2);

/** Clips r * factor to the range [LO,HI].
  */
template <int LO, int HI>
inline int clip(double r, double factor)
{
	int a = (int)round(r * factor);
	return std::min(std::max(a, LO), HI);
}

/** Calculate greatest common divider of two strictly positive integers.
  * Classical implementation is like this:
  *    while (unsigned t = b % a) { b = a; a = t; }
  *    return a;
  * The following implementation avoids the costly modulo operation. It
  * is about 40% faster on my machine.
  *
  * require: a != 0  &&  b != 0
  */
inline unsigned gcd(unsigned a, unsigned b)
{
	unsigned k = 0;
	while (((a & 1) == 0) && ((b & 1) == 0)) {
		a >>= 1; b >>= 1; ++k;
	}

	// either a or b (or both) is odd
	while ((a & 1) == 0) a >>= 1;
	while ((b & 1) == 0) b >>= 1;

	// both a and b odd
	while (a != b) {
		if (a >= b) {
			a -= b;
			do { a >>= 1; } while ((a & 1) == 0);
		} else {
			b -= a;
			do { b >>= 1; } while ((b & 1) == 0);
		}
	}
	return b << k;
}

} // namespace Math

} // namespace openmsx

#endif // MATH_HH
