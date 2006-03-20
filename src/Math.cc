// $Id$

#include "Math.hh"

namespace openmsx {

namespace Math {

unsigned powerOfTwo(unsigned a)
{
	unsigned res = 1;
	while (a > res) res <<= 1;
	return res;
}

void gaussian2(double& r1, double& r2)
{
	static const double S = 2.0 / RAND_MAX;
	double x1, x2, w;
	do {
		x1 = S * rand() - 1.0;
		x2 = S * rand() - 1.0;
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0);
	w = sqrt((-2.0 * log(w)) / w);
	r1 = x1 * w;
	r2 = x2 * w;
}

} // namespace Math

} // namespace openmsx
