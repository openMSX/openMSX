#ifndef FFTREAL_HH
#define FFTREAL_HH

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
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>

// Input mode for FFT optimization
enum class FFTInputMode {
	Full,           // All FFT_LEN input values provided
	                // Output: N complex values in FFTReal format (N/2+1 real, N/2-1 imag)
	HalfZeroPadded, // Only first FFT_LEN/2 values provided, second half is implicitly zero
	                // Example N=8: provide [a,b,c,d] → full array [a,b,c,d,0,0,0,0]
	                // Output: N complex values in FFTReal format (N/2+1 real, N/2-1 imag)
	Symmetric       // Only first FFT_LEN/2+1 values, mirrored to fill second half
	                // input[0] and input[N/2] are unique (endpoints)
	                // input[1..N/2-1] mirrored: input[N/2+i] = input[N/2-i]
	                // Example N=8: provide [a,b,c,d,e] → full array [a,b,c,d,e,d,c,b]
	                // Output: Only N/2+1 REAL values (imaginary parts are ~0 and skipped)
};

// Calculate the FFT for real input (as opposed to complex numbers input).
// NOTE: Uses exp(+2πi) sign convention (opposite from numpy/FFTW). Imaginary parts have opposite sign.
//
// The result however are still complex numbers, and arranged as follows:
//   output[0]         = Real(bin 0)
//   output[1]         = Real(bin 1)
//   ...
//   output[len/2 - 1] = Real(bin len/2 -1)
//   output[len/2    ] = Real(bin len/2)
//   output[len/2 + 1] = Imag(bin 1)
//   ...
//   output[len   - 2] = Imag(bin len/2 - 2)
//   output[len   - 1] = Imag(bin len/2 - 1)
// Note: There are two more real-parts than imaginary-parts. For the lowest bin
// (the DC-offset) and for the highest bin (at Nyquist frequency) the output is
// a pure real value (the imaginary part is zero).
template<unsigned FFT_LEN_L2, FFTInputMode MODE = FFTInputMode::Full>
class FFTReal
{
public:
	static_assert(FFT_LEN_L2 >=  4); // at least 16-points (allows optimization in final pass)
	static_assert(FFT_LEN_L2 <= 16); // at most 65536-points
	static constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2;
	static constexpr unsigned INPUT_LEN =
		(MODE == FFTInputMode::HalfZeroPadded) ? (FFT_LEN / 2) :
		(MODE == FFTInputMode::Symmetric) ? (FFT_LEN / 2 + 1) :
		FFT_LEN;
	static constexpr unsigned OUTPUT_LEN =
		(MODE == FFTInputMode::Symmetric) ? (FFT_LEN / 2 + 1) :
		FFT_LEN;

	// Execute FFT and return meaningful output range
	// NOTE: The returned span aliases the 'output' parameter (first OUTPUT_LEN elements)
	//       Full 'output' buffer is still needed for intermediate calculations
	static std::span<float, OUTPUT_LEN> execute(std::span<const float, INPUT_LEN> input,
	                                            std::span<float, FFT_LEN> output,
	                                            std::span<float, FFT_LEN> tmpBuf)
	{
		pass<FFT_LEN_L2 - 1>(output, tmpBuf, input);
#ifndef NDEBUG
		// In debug mode, verify imaginary outputs are indeed ~0 for Symmetric mode
		if constexpr (MODE == FFTInputMode::Symmetric) {
			for (unsigned i = FFT_LEN / 2 + 1; i < FFT_LEN; ++i) {
				assert(std::abs(output[i]) < 1e-4f && "Symmetric mode: imaginary outputs should be ~0");
			}
		}
#endif
		return std::span<float, OUTPUT_LEN>(output.data(), OUTPUT_LEN);
	}

private:
	// Helper for dependent false in static_assert
	template<auto> static inline constexpr bool always_false = false;

	template<int PASS>
	static void pass(std::span<float, FFT_LEN> dst,
	                 std::span<float, FFT_LEN> src,
	                 std::span<const float, INPUT_LEN> input)
	{
		if constexpr (PASS == 1) {
			// Do 1st and 2nd pass at once.
			static constexpr auto len4 = FFT_LEN >> 2;

			if constexpr (MODE == FFTInputMode::Full) {
				// Full input path (general case)
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
			} else if constexpr (MODE == FFTInputMode::HalfZeroPadded) {
				// Optimized path: second (implied) half of input is zero
				// ri_1 and ri_3 always point to second half, so input[ri_1] == input[ri_3] == 0
				// This is a simplified version of the Full case above
				for (unsigned idx = 0; idx < FFT_LEN; idx += 4) {
					const unsigned ri_0 = bitRevBuf[idx >> 2];
					const unsigned ri_2 = ri_0 + 1 * len4; // bitRevBuf[idx + 2];

					dst[idx + 1] = input[ri_0];
					dst[idx + 3] = input[ri_2];
					dst[idx + 0] = input[ri_0] + input[ri_2];
					dst[idx + 2] = input[ri_0] - input[ri_2];
				}
			} else if constexpr (MODE == FFTInputMode::Symmetric) {
				// Symmetric input: input[0..N/2] provided, input[k] = input[N-k] for k > N/2
				// ri_0 and ri_2 always < N/2 (first half, use directly)
				// ri_1 and ri_3 always >= N/2 (second half, use mirrored index)
				for (unsigned idx = 0; idx < FFT_LEN; idx += 4) {
					const unsigned ri_0 = bitRevBuf[idx >> 2];
					const unsigned ri_1 = ri_0 + 2 * len4; // bitRevBuf[idx + 1];
					const unsigned ri_2 = ri_0 + 1 * len4; // bitRevBuf[idx + 2];
					const unsigned ri_3 = ri_0 + 3 * len4; // bitRevBuf[idx + 3];

					const float in_0 = input[ri_0];
					const float in_1 = input[FFT_LEN - ri_1];
					const float in_2 = input[ri_2];
					const float in_3 = input[FFT_LEN - ri_3];

					dst[idx + 1] = in_0 - in_1;
					dst[idx + 3] = in_2 - in_3;

					const float sf_0 = in_0 + in_1;
					const float sf_2 = in_2 + in_3;

					dst[idx + 0] = sf_0 + sf_2;
					dst[idx + 2] = sf_0 - sf_2;
				}
			} else {
				static_assert(always_false<MODE>, "Unsupported FFTInputMode");
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

			// For Symmetric input mode on final pass, skip imaginary outputs (they're ~0)
			// Final pass: dist = FFT_LEN/4, so c2_i and positions >= c2_r + 1 are imaginary
			// Only skip in release mode; in debug mode we compute them to validate they're ~0
			[[maybe_unused]] static constexpr bool isFinalPass = (PASS == FFT_LEN_L2 - 1);
#ifdef NDEBUG
			static constexpr bool skipImaginary = (MODE == FFTInputMode::Symmetric) && isFinalPass;
#else
			static constexpr bool skipImaginary = false;
#endif

			for (unsigned idx = 0; idx < FFT_LEN; idx += cend) {
				std::span<const float> sf = src.subspan(idx);
				std::span<      float> df = dst.subspan(idx);

				// Extreme coefficients are always real
				df[c1_r] = sf[c1_r] + sf[c2_r];
				df[c2_r] = sf[c1_r] - sf[c2_r];
				df[c1_i] = sf[c1_i];
				if constexpr (!skipImaginary) {
					df[c2_i] = sf[c2_i];  // Imaginary section
				}

				// Others are conjugate complex numbers
				for (unsigned i = 1; i < dist; ++ i) {
					const float c = cosBuf[        i  * table_step];
					const float s = cosBuf[(dist - i) * table_step];

					const float sf_r_i = sf[c1_r + i];
					const float sf_i_i = sf[c1_i + i];

					const float v1 = sf[c2_r + i] * c - sf[c2_i + i] * s;
					df[c1_r + i] = sf_r_i + v1;
					df[c2_r - i] = sf_r_i - v1;

					if constexpr (!skipImaginary) {
						const float v2 = sf[c2_r + i] * s + sf[c2_i + i] * c;
						df[c2_r + i] = v2 + sf_i_i;
						df[cend - i] = v2 - sf_i_i;
					}
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
