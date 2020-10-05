#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <tuple>

static constexpr bool EG_STEP_HI[4][4] = {
	{ 0, 1, 1, 1 },
	{ 0, 0, 0, 1 },
	{ 0, 0, 1, 1 },
	{ 0, 0, 0, 0 }
};

constexpr struct EnvTables {
	uint8_t attack [14][4][64];
	uint8_t release[14][4][4][64];
} envTabs = []() {
	EnvTables r = {};
	for (int timer_shift = 0; timer_shift < 14; ++timer_shift) {
		for (int timer = 0; timer < 4; ++timer) {
			for (int rate = 0; rate < 64; ++rate) {
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
					if ((rate_hi != 0xf)) {
						int32_t shift = inc_lo ? 1
						                       : rate_hi - 11 + inc_hi;
						if (shift > 0) {
							return 5 - std::min(4, shift);
						}
					}
					return 31;
				}();
				for (int counter_state = 0; counter_state < 4; ++counter_state) {
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

constexpr void copy_64(const uint8_t* in, uint8_t* out) // TODO use c++20 std::copy()
{
	for (int i = 0; i < 64; ++i) {
		out[i] = in[i];
	}
}
constexpr bool equal_64(const uint8_t* tab1, const uint8_t* tab2) // TODO use c++20 std::equal()
{
	for (int i = 0; i < 64; ++i) {
		if (tab1[i] != tab2[i]) return false;
	}
	return true;
}
constexpr int find_64(const uint8_t* needle, const uint8_t (*haystack)[64], int max_n) // TODO use c++20 std::find
{
	for (int i = 0; i < max_n; ++i) {
		if (equal_64(needle, haystack[i])) return i;
	}
	return -1;
}

//constexpr // fine for clang, but not for gcc ???
struct CompressedEnvTables {
	uint8_t releaseIndex[14][4][4];
	uint8_t releaseData[64][64];  // '64' (size after compression) found by trial and error
} ctab = []() {
	CompressedEnvTables r = {};
	//compressTable(envTabs.release, r.releaseIndex, r.releaseData);
	int out_n = 0;
	for (int i = 0; i < 14; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				int f = find_64(envTabs.release[i][j][k], r.releaseData, out_n);
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
	for (int timer_shift = 0; timer_shift < 14; ++timer_shift) {
		for (int timer = 0; timer < 4; ++timer) {
			if (timer == 0) {
				std::cout << " {{";
			} else {
				std::cout << "  {";
			}
			for (int rate = 0; rate < 64; ++rate) {
				int r = envTabs.attack[timer_shift][timer][rate];
				if (r != 31) {
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
	for (int timer_shift = 0; timer_shift < 14; ++timer_shift) {
		std::cout << " {";
		for (int timer = 0; timer < 4; ++timer) {
			std::cout << '{';
			for (int counter_state = 0; counter_state < 4; ++counter_state) {
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
	for (int idx = 0; idx < 64; ++idx) {
		std::cout << " {";
		for (int rate = 0; rate < 64; ++rate) {
			int r = ctab.releaseData[idx][rate];
			if (r != 0) {
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
