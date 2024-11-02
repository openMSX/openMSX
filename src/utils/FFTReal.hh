#ifndef FFTREAL_HH
#define	FFTREAL_HH

// This code is based on:
//    fft-real
//    https://github.com/cyrilcode/fft-real
//    FFTReal is distributed under the terms of the Do What The Fuck You Want To Public License.
//
// This version is stripped down and specialized for our use case.

#include "cstd.hh"
#include "narrow.hh"
#include "xrange.hh"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

// Calculate the FFT for real input (as opposed to complex numbers input).
// The result however are still complex numbers, and arranged as follows:
//   output[0]         = Real(bin 0)
//   output[1]         = Real(bin 1)
//   ...
//   output[len/2 - 1] = Real(bin len/2 -1)
//   output[len/2    ] = Real(bin len/2)
//   output[len/2 + 1] = Imag(bin 1)
//   ...
//   output[len   - 2] = Imag(bin len/2 - 2)
//   output[len   - 1] = Real(bin len/2 - 1)
// Note: There are two more real-parts than imaginary-parts. For the lowest bin
// (the DC-offset) and for the highest bin (at Nyquist frequency) the output is
// a pure real value (the imaginary part is zero).
template<unsigned FFT_LEN_L2>
class FFTReal
{
public:
	static_assert(FFT_LEN_L2 >=  3); // at least 8-points
	static_assert(FFT_LEN_L2 <= 16); // at most 65536-points
	static constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2;

	static void execute(std::span<const float, FFT_LEN> input,
	                    std::span<float, FFT_LEN> output,
	                    std::span<float, FFT_LEN> tmpBuf)
	{
		pass<FFT_LEN_L2 - 1>(output, tmpBuf, input);
	}

private:
	template<int PASS>
	static void pass(std::span<float, FFT_LEN> dst,
	                 std::span<float, FFT_LEN> src,
	                 std::span<const float, FFT_LEN> input)
	{
		if constexpr (PASS == 1) {
			// Do 1st and 2nd pass at once.
			static constexpr auto len4 = FFT_LEN >> 2;
			for (unsigned idx = 0; idx < FFT_LEN; idx += 4) {
				const unsigned ri_0 = bitRevBuf[idx >> 2];
				const unsigned ri_1 = ri_0 + 2 * len4; // bitRevBuf[idx + 1];
				const unsigned ri_2 = ri_0 + 1 * len4; // bitRevBuf[idx + 2];
				const unsigned ri_3 = ri_0 + 3 * len4; // bitRevBuf[idx + 3];

				dst[idx + 1] = input[ri_0] - input[ri_1];
				dst[idx + 3] = input[ri_2] - input[ri_3];

				const float sf_0 = input[ri_0] + input[ri_1];
				const float sf_2 = input[ri_2] + input[ri_3];

				dst[idx + 0] = sf_0 + sf_2;
				dst[idx + 2] = sf_0 - sf_2;
			}

		} else if constexpr (PASS == 2) {
			// Start with 1st and 2nd passes (swap source and destination buffers).
			pass<1>(src, dst, input);

			// 3rd pass
			static constexpr float sqrt2_2 = std::numbers::sqrt2_v<float> * 0.5f;
			for (unsigned idx = 0; idx < FFT_LEN; idx += 8) {
				dst[idx + 0] = src[idx] + src[idx + 4];
				dst[idx + 4] = src[idx] - src[idx + 4];
				dst[idx + 2] = src[idx + 2];
				dst[idx + 6] = src[idx + 6];

				float v1 = (src[idx + 5] - src[idx + 7]) * sqrt2_2;
				dst[idx + 1] = src[idx + 1] + v1;
				dst[idx + 3] = src[idx + 1] - v1;

				float v2 = (src[idx + 5] + src[idx + 7]) * sqrt2_2;
				dst[idx + 5] = v2 + src[idx + 3];
				dst[idx + 7] = v2 - src[idx + 3];
			}

		} else {
			// First do the previous passes (swap source and destination buffers).
			pass<PASS - 1>(src, dst, input);

			static constexpr unsigned dist = 1 << (PASS - 1);
			static constexpr unsigned c1_r = 0;
			static constexpr unsigned c1_i = dist * 1;
			static constexpr unsigned c2_r = dist * 2;
			static constexpr unsigned c2_i = dist * 3;
			static constexpr unsigned cend = dist * 4;
			static constexpr unsigned table_step = COS_ARR_SIZE >> (PASS - 1);

			for (unsigned idx = 0; idx < FFT_LEN; idx += cend) {
				std::span<const float> sf = src.subspan(idx);
				std::span<      float> df = dst.subspan(idx);

				// Extreme coefficients are always real
				df[c1_r] = sf[c1_r] + sf[c2_r];
				df[c2_r] = sf[c1_r] - sf[c2_r];
				df[c1_i] = sf[c1_i];
				df[c2_i] = sf[c2_i];

				// Others are conjugate complex numbers
				for (unsigned i = 1; i < dist; ++ i) {
					const float c = cosBuf[        i  * table_step];
					const float s = cosBuf[(dist - i) * table_step];

					const float sf_r_i = sf[c1_r + i];
					const float sf_i_i = sf[c1_i + i];

					const float v1 = sf[c2_r + i] * c - sf[c2_i + i] * s;
					df[c1_r + i] = sf_r_i + v1;
					df[c2_r - i] = sf_r_i - v1;

					const float v2 = sf[c2_r + i] * s + sf[c2_i + i] * c;
					df[c2_r + i] = v2 + sf_i_i;
					df[cend - i] = v2 - sf_i_i;
				}
			}
		}
	}

	static constexpr auto bitRevBuf = []{
		constexpr int BR_ARR_SIZE = FFT_LEN / 4;
		std::array<uint16_t, BR_ARR_SIZE> result = {};
		for (auto cnt : xrange(narrow<unsigned>(result.size()))) {
			unsigned index = cnt << 2;
			unsigned res = 0;
			for (int bit_cnt = FFT_LEN_L2; bit_cnt > 0; --bit_cnt) {
				res <<= 1;
				res += (index & 1);
				index >>= 1;
			}
			result[cnt] = narrow<uint16_t>(res);
		}
		return result;
	}();

	static constexpr int COS_ARR_SIZE = FFT_LEN / 4;
	static constexpr auto cosBuf = []{
		std::array<float, COS_ARR_SIZE> result = {};
		const double mul = (0.5 * std::numbers::pi) / COS_ARR_SIZE;
		for (auto i : xrange(narrow<unsigned>(result.size()))) {
			result[i] = float(cstd::cos<4>(i * mul));
		}
		return result;
	}();
};

#endif
