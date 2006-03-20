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

} // namespace Math

} // namespace openmsx

#endif // MATH_HH
