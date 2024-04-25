#include "enumerate.hh"
#include "narrow.hh"
#include "xrange.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <span>

using B4 = std::array<bool, 4>;
static constexpr std::array<B4, 4> EG_STEP_HI = {
	B4{false, true , true , true },
	B4{false, false, false, true },
	B4{false, false, true , true },
	B4{false, false, false, false},
};

static constexpr struct EnvTables {
	std::array<std::array<std::array<uint8_t, 64/*rate*/>, 4/*timer*/>, 14/*timer_shift*/> attack;
	std::array<std::array<std::array<std::array<uint8_t, 64/*rate*/>, 4/*counter_state*/>, 4/*timer*/>, 14/*timer_shift*/> release;
} envTabs = []() {
	EnvTables r = {};
	for (auto timer_shift : xrange(14)) {
		for (auto timer : xrange(4)) {
			for (auto rate : xrange(64)) {
				uint8_t rate_hi = rate >> 2;
				uint8_t rate_lo = rate & 3;
				bool inc_hi = EG_STEP_HI[timer][rate_lo];
				bool inc_lo = [&]() -> bool {
					if ((rate_hi < 12) && (rate_hi != 0)) {
						switch (timer_shift + rate_hi) {
							case 12: return true;
							case 13: return rate_lo & 2;
							case 14: return rate_lo & 1;
						}
					}
					return false;
				}();
				r.attack[timer_shift][timer][rate] = [&]() {
					if (rate_hi != 0xf) {
						int32_t shift = (rate_hi < 12) ? inc_lo
						                               : (rate_hi - 11 + inc_hi);
						if (shift > 0) {
							return 5 - shift;
						}
					}
					return 31;
				}();
				for (auto counter_state : xrange(4)) {
					bool i0 = rate_hi == 15 || (rate_hi == 14 && inc_hi);
					bool i1 = (rate_hi == 14 && !inc_hi)
					       || (rate_hi == 13 && inc_hi)
					       || (rate_hi == 13 && !inc_hi && (counter_state & 1))
					       || (rate_hi == 12 && inc_hi && (counter_state & 1))
					       || (rate_hi == 12 && !inc_hi && (counter_state == 3))
					       || (inc_lo && (counter_state == 3));
					int i01 = (i0 << 1) | i1;
					r.release[timer_shift][timer][counter_state][rate] = i01;
				}
			}
		}
	}
	return r;
}();

constexpr void copy_64(std::span<const uint8_t, 64> in, std::span<uint8_t, 64> out) // TODO use c++20 std::copy()
{
	for (auto i : xrange(64)) {
		out[i] = in[i];
	}
}
constexpr bool equal_64(std::span<const uint8_t, 64> tab1, std::span<const uint8_t, 64> tab2) // TODO use c++20 std::equal()
{
	for (auto i : xrange(64)) {
		if (tab1[i] != tab2[i]) return false;
	}
	return true;
}
constexpr int find_64(std::span<const uint8_t, 64> needle, std::span<std::array<uint8_t, 64>> haystack) // TODO use c++20 std::find
{
	for (auto [i, candidate] : enumerate(haystack)) {
		if (equal_64(needle, candidate)) return narrow<int>(i);
	}
	return -1;
}

//constexpr // fine for clang, but not for gcc ???
struct CompressedEnvTables {
	std::array<std::array<std::array<uint8_t, 4>, 4>, 14> releaseIndex;
	std::array<std::array<uint8_t, 64>, 64> releaseData;  // '64' (size after compression) found by trial and error
} ctab = []() {
	CompressedEnvTables r = {};
	//compressTable(envTabs.release, r.releaseIndex, r.releaseData);
	size_t out_n = 0;
	for (auto i : xrange(14)) {
		for (auto j : xrange(4)) {
			for (auto k : xrange(4)) {
				int f = find_64(envTabs.release[i][j][k], std::span{r.releaseData.data(), out_n});
				if (f == -1) {
					copy_64(envTabs.release[i][j][k], r.releaseData[out_n]);
					r.releaseIndex[i][j][k] = out_n;
					++out_n;
				} else {
					r.releaseIndex[i][j][k] = f;
				}
			}
		}
	}
	assert(out_n == 64);
	return r;
}();

int main()
{
	std::cout << "#define _ 31\n"
		     "constexpr uint8_t attack[14][4][64] = {\n";
	for (auto timer_shift : xrange(14)) {
		for (auto timer : xrange(4)) {
			if (timer == 0) {
				std::cout << " {{";
			} else {
				std::cout << "  {";
			}
			for (auto rate : xrange(64)) {
				if (int r = envTabs.attack[timer_shift][timer][rate];
				    r != 31) {
					std::cout << r;
				} else {
					std::cout << '_';
				}
				if (rate != 63) std::cout << ',';
			}
			if (timer != 3) {
				std::cout << "},\n";
			} else {
				std::cout << "}},\n";
			}
		}
	}
	std::cout << "};\n"
	             "#undef _\n\n";

	std::cout << "constexpr uint8_t releaseIndex[14][4][4] = {\n";
	for (auto timer_shift : xrange(14)) {
		std::cout << " {";
		for (auto timer : xrange(4)) {
			std::cout << '{';
			for (auto counter_state : xrange(4)) {
				auto idx = ctab.releaseIndex[timer_shift][timer][counter_state];
				std::cout << int(idx);
				if (counter_state != 3) std::cout << ',';
			}
			if (timer != 3) {
				std::cout << "}, ";
			} else {
				std::cout << '}';
			}
		}
		std::cout << "},\n";
	}
	std::cout << "};\n\n";

	std::cout << "#define _ 0\n"
	             "constexpr uint8_t releaseData[64][64] = {\n";
	for (auto idx : xrange(64)) {
		std::cout << " {";
		for (auto rate : xrange(64)) {
			if (int r = ctab.releaseData[idx][rate];
			    r != 0) {
				std::cout << r;
			} else {
				std::cout << '_';
			}
			if (rate != 63) std::cout << ',';
		}
		std::cout << "},\n";
	}
	std::cout << "};\n"
	             "#undef _\n";
}
