#ifndef FAST_LOG2_HH
#define FAST_LOG2_HH

// Based on:
//   fast_log
//     https://github.com/nadavrot/fast_log/tree/main
//   MIT License
//   Copyright (c) 2022 Nadav Rotem
//
// This version is modified/simplified to better suite openMSX:
// * double -> float
// * simplified fast_frexpf()  (no longer works for negative or zero values)
// * calculate log base-2 instead of natural-log.

// Main advantage over std::log() or std::log2():
// * A lot faster, and can be (auto-)vectorized (verified gcc-11 and above)
// Main disadvantage:
// * Not as accurate, max error: ~0.25%. std::log() is accurate to 1 ULP.

#include <bit>
#include <cstdint>
#include <utility>

// Similar to std::frexp():
//   decompose 'x' into 'm' and 'e' so that
//     x == m * pow(2, e)
//   with: 0.5 <= m < 1.0
// This function has some limitations
// * It doesn't work at all for negative values.
// * It gives an inaccurate result for x=0 (it returns m=0.5, e=-126, that's
//   very close but not exact).
// These limitations allow this version to be much faster: it can easily be
// inlined and it's suitable for (auto-)vectorization.
inline std::pair<float, int> fast_frexpf(float x)
{
	// See: https://en.wikipedia.org/wiki/IEEE_754#Basic_and_interchange_formats
	auto bits = std::bit_cast<uint32_t>(x);
	//if (bits == 0) return {0.0f, 0};
	auto mantissa = bits & 0x007fffff;
	auto exponent = bits >> 23; // note: wrong if x < 0
	auto frac = std::bit_cast<float>(mantissa | 0x3f000000);
	return {frac, exponent - 126};
}

// Compute a fast _approximation_ for the base-2 logarithm of 'x'.
inline float fast_log2(float x)
{
	auto [m, exp] = fast_frexpf(x); // split x into: x == pow(2, exp) * m

	// Use a degree-3 polynomial to approximate log2(m) over the interval [0.5, 1)
	//  val = a*pow(m,3) + b*pow(m,2) + c*m + d
	static constexpr float a =  1.33755322f;
	static constexpr float b = -4.42852392f;
	static constexpr float c =  6.30371424f;
	static constexpr float d = -3.21430967f;
	float val = d + m * (c + m * (b + m * a));

	return float(exp) + val;
}

#endif
