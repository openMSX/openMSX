#include "catch.hpp"
#include "ResampleHQ.hh"

#include "enumerate.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

using namespace openmsx;

// The prototype filter used to be a table of 2463 values (ResampleCoeffs.ii,
// copied from libsamplerate's fastest_coeffs.h). It's now calculated from the
// (reverse engineered) formula. These are a few values from that table: the
// formula must reproduce them (within the precision of the table's floats).
TEST_CASE("ResampleCoeffs, prototype reproduces the libsamplerate table")
{
	struct Ref { int index; double value; };
	static constexpr std::array refs = {
		Ref{   0,  8.3147237295e-01},
		Ref{   1,  8.3141400554e-01},
		Ref{   2,  8.3123891827e-01},
		Ref{  10,  8.2564796911e-01},
		Ref{  77,  5.2666910454e-01},
		Ref{ 128,  1.5869923242e-01},
		Ref{ 154, -2.9625626534e-04},
		Ref{ 500, -4.6904023267e-02},
		Ref{1000,  1.7897681038e-02},
		Ref{1231, -1.0417765262e-04},
		Ref{1500, -2.7378821066e-03},
		Ref{2000,  1.0484915791e-05},
		Ref{2300,  8.6722042004e-06},
		Ref{2400, -1.4611523418e-05},
		Ref{2461, -2.3895239801e-07},
		Ref{2462, -1.2288967738e-07},
	};
	for (const auto& [index, value] : refs) {
		CHECK(std::abs(ResampleCoeffs::coeff(index) - value) < 1e-7);
	}
}

TEST_CASE("ResampleCoeffs, prototype properties")
{
	using RC = ResampleCoeffs;

	SECTION("symmetric, maximum at 0") {
		CHECK(RC::coeff(0.0) > RC::coeff(0.5));
		for (double x : {0.3, 1.0, 77.7, 1000.0, 2461.9}) {
			CHECK(RC::coeff(x) == RC::coeff(-x));
			CHECK(std::abs(RC::coeff(x)) < RC::coeff(0.0));
		}
	}
	SECTION("finite support") {
		CHECK(RC::coeff(RC::HALF_LEN) != 0.0);
		CHECK(RC::coeff(RC::HALF_LEN + 0.001) == 0.0);
		CHECK(RC::coeff(1e9) == 0.0);
	}
	SECTION("zero crossings of the sinc") {
		for (int k : xrange(1, 16)) {
			CHECK(std::abs(RC::coeff(k * RC::PERIOD)) < 1e-12);
		}
		// and in between the sign alternates
		CHECK(RC::coeff(1.5 * RC::PERIOD) < 0.0);
		CHECK(RC::coeff(2.5 * RC::PERIOD) > 0.0);
		CHECK(RC::coeff(3.5 * RC::PERIOD) < 0.0);
	}
	SECTION("normalization: integer taps sum to INDEX_INC") {
		double sum = RC::coeff(0.0);
		for (int i : xrange(1, RC::HALF_LEN + 1)) {
			sum += 2.0 * RC::coeff(i);
		}
		CHECK(std::abs(sum - RC::INDEX_INC) < 1e-9);
	}
	SECTION("fudge(): reproduces the original design, wider window -> higher cutoff") {
		double L0 = double(RC::HALF_LEN) / RC::INDEX_INC;
		CHECK(std::abs(RC::fudge(L0) - RC::FUDGE) < 1e-12);
		CHECK(RC::fudge(1.1 * L0) < RC::FUDGE);
		CHECK(RC::fudge(0.9 * L0) > RC::FUDGE);
		CHECK(RC::fudge(1e9) > 1.0); // cutoff is always below Nyquist
	}
}

// Magnitude response (in dB, relative to DC) of a FIR filter at frequency 'f'
// (the filter runs at sample rate 'rate').
static double response(std::span<const float> taps, double f, double rate)
{
	double dc = 0.0, re = 0.0, im = 0.0;
	for (auto [j, c] : enumerate(taps)) {
		double ph = 2.0 * std::numbers::pi * f * double(j) / rate;
		dc += c;
		re += c * std::cos(ph);
		im += c * std::sin(ph);
	}
	return 20.0 * std::log10(std::hypot(re, im) / dc);
}

static void testTable(double inputRate, unsigned expectedFilterLen, double expectedMinus3dB)
{
	constexpr double HOST = 44100.0;
	constexpr size_t H = ResampleHQ<1>::HALF_TAB_LEN;
	double ratio = inputRate / HOST;

	auto& coeffs = ResampleCoeffs::instance();
	float* table; unsigned filterLen;
	coeffs.getCoeffs(ratio, table, filterLen);
	REQUIRE(filterLen == expectedFilterLen);
	REQUIRE((filterLen % 8) == 0);

	// table[0] is row -1, row 't' is at (t + 1) * filterLen, there are
	// HALF_TAB_LEN + 2 rows in total
	auto row = [&](int t) { return std::span{table + (t + 1) * filterLen, filterLen}; };

	SECTION("rows sum to 1, consecutive rows are close") {
		for (int t : xrange(-1, int(H) + 1)) {
			auto r = row(t);
			double sum = 0.0;
			for (auto c : r) sum += c;
			CHECK(std::abs(sum - 1.0) < 1e-5); // ~5e-6: leakage at the input rate
			if (t < int(H)) {
				auto n = row(t + 1);
				double maxDiff = 0.0;
				for (auto j : xrange(filterLen)) {
					maxDiff = std::max(maxDiff, double(std::abs(n[j] - r[j])));
				}
				CHECK(maxDiff < 0.01);
			}
		}
	}
	SECTION("extra rows") {
		// row HALF_TAB_LEN is row HALF_TAB_LEN-1 mirrored
		auto a = row(int(H) - 1), b = row(int(H));
		for (auto j : xrange(filterLen)) {
			CHECK(b[j] == a[filterLen - 1 - j]);
		}
		// row -1 is row 0 mirrored and shifted by one tap
		auto m = row(-1), z = row(0);
		for (auto j : xrange(filterLen - 1)) {
			CHECK(m[j] == z[filterLen - 2 - j]);
		}
		CHECK(m[filterLen - 1] == 0.0f);
	}
	SECTION("frequency response") {
		// The filter runs at the input rate. Its cutoff is relative to
		// the output rate when downsampling, relative to the input rate
		// when upsampling.
		double ref = std::min(inputRate, HOST);
		auto r = row(0);
		for (double f = 0.01 * ref; f <= 0.34 * ref; f += 0.01 * ref) {
			CHECK(std::abs(response(r, f, inputRate)) < 0.01); // pass band
		}
		CHECK(std::abs(response(r, expectedMinus3dB * ref, inputRate) - (-3.0)) < 0.05); // -3dB
		if (ratio > 1.0) {
			for (double f = 0.5 * ref; f <= 0.5 * inputRate; f += 0.001 * inputRate) {
				CHECK(response(r, f, inputRate) < -97.5); // stop band
			}
		}
	}
	SECTION("cache") {
		float* table2; unsigned filterLen2;
		coeffs.getCoeffs(ratio, table2, filterLen2);
		CHECK(table2 == table);
		CHECK(filterLen2 == filterLen);
		coeffs.releaseCoeffs(ratio);
	}
	coeffs.releaseCoeffs(ratio);
}

TEST_CASE("ResampleCoeffs, tables")
{
	// The -3dB points follow from the window span, see the comments in
	// ResampleHQ.cc (0.4036 * 44100 = 17.8kHz etc.)
	SECTION("PSG")     { testTable(223722.0, 200, 0.4036); } // downsample 5.07x
	SECTION("OPLL")    { testTable( 49716.0,  48, 0.4107); } // downsample 1.13x
	SECTION("VLM5030") { testTable(  8135.0,  40, 0.4050); } // upsample
}
