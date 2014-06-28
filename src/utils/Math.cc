#include "Math.hh"

namespace Math {

unsigned powerOfTwo(unsigned a)
{
	// classical implementation:
	//   unsigned res = 1;
	//   while (a > res) res <<= 1;
	//   return res;

	// optimized version
	a += (a == 0); // can be removed if argument is never zero
	return floodRight(a - 1) + 1;
}

} // namespace Math
