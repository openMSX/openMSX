#include "catch.hpp"
#include "FFTReal.hh"

#include <array>
#include <cmath>

// Compare two arrays with tolerance, return max error
static float compareArrays(const float* a, const float* b, size_t size) {
	float maxError = 0.0f;
	for (size_t i = 0; i < size; ++i) {
		float error = std::abs(a[i] - b[i]);
		maxError = std::max(maxError, error);
	}
	return maxError;
}

TEST_CASE("FFTReal: Full mode - known patterns")
{
	constexpr float tolerance = 1e-5f;

	SECTION("All zeros") {
		constexpr unsigned FFT_LEN_L2 = 5;
		constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 32

		std::array<float, FFT_LEN> input = {};
		std::array<float, FFT_LEN> output, tmpBuf;

		auto result = FFTReal<FFT_LEN_L2>::execute(input, output, tmpBuf);

		// FFT of all zeros should be all zeros
		for (unsigned i = 0; i < FFT_LEN; ++i) {
			CHECK(std::abs(result[i]) < tolerance);
		}
	}

	SECTION("Impulse at t=0") {
		constexpr unsigned FFT_LEN_L2 = 5;
		constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 32

		std::array<float, FFT_LEN> input = {};
		input[0] = 1.0f;
		std::array<float, FFT_LEN> output, tmpBuf;

		auto result = FFTReal<FFT_LEN_L2>::execute(input, output, tmpBuf);

		// FFT of impulse should be constant (DC = 1, all others = 0 imaginary)
		// Real parts should all be 1.0
		CHECK(std::abs(result[0] - 1.0f) < tolerance); // DC
		for (unsigned i = 1; i <= FFT_LEN / 2; ++i) {
			CHECK(std::abs(result[i] - 1.0f) < tolerance);
		}
		// Imaginary parts should all be ~0
		for (unsigned i = FFT_LEN / 2 + 1; i < FFT_LEN; ++i) {
			CHECK(std::abs(result[i]) < tolerance);
		}
	}

	SECTION("Constant signal") {
		constexpr unsigned FFT_LEN_L2 = 5;
		constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 32

		std::array<float, FFT_LEN> input;
		for (unsigned i = 0; i < FFT_LEN; ++i) {
			input[i] = 1.0f;
		}
		std::array<float, FFT_LEN> output, tmpBuf;

		auto result = FFTReal<FFT_LEN_L2>::execute(input, output, tmpBuf);

		// FFT of constant should have all energy in DC bin
		CHECK(std::abs(result[0] - float(FFT_LEN)) < tolerance);
		for (unsigned i = 1; i < FFT_LEN; ++i) {
			CHECK(std::abs(result[i]) < tolerance);
		}
	}
}

TEST_CASE("FFTReal: Full mode - fixed random pattern")
{
	// This test uses a fixed random pattern (seed=42, size=32) and expected
	// output calculated with numpy.fft.rfft([...])
	constexpr float tolerance = 1e-4f;
	constexpr unsigned FFT_LEN_L2 = 5;
	constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 32

	// Input: 32 random values (seed=42, range [-1, 1])
	// Generated with: np.random.seed(42); np.random.uniform(-1, 1, 32)
	std::array<float, FFT_LEN> input = {
		-0.2509198f,  0.9014286f,  0.4639879f,  0.1973170f,
		-0.6879627f, -0.6880109f, -0.8838328f,  0.7323523f,
		 0.2022300f,  0.4161451f, -0.9588310f,  0.9398197f,
		 0.6648853f, -0.5753218f, -0.6363501f, -0.6331910f,
		-0.3915155f,  0.0495129f, -0.1361100f, -0.4175417f,
		 0.2237058f, -0.7210123f, -0.4157107f, -0.2672763f,
		-0.0878600f,  0.5703519f, -0.6006525f,  0.0284689f,
		 0.1848291f, -0.9070992f,  0.2150897f, -0.6589518f
	};

	std::array<float, FFT_LEN> output, tmpBuf;
	auto result = FFTReal<FFT_LEN_L2>::execute(input, output, tmpBuf);

	// Expected output from numpy.fft.rfft() - stored in FFTReal format
	// NOTE: FFTReal uses exp(+2πi) convention, so imaginary parts are negated from numpy
	// Real parts: indices 0 to N/2 (17 values)
	// Imaginary parts: indices N/2+1 to N-1 (15 values, negated from numpy)
	std::array<float, FFT_LEN> expected = {
		// Real parts [0 .. 16]
		-4.1280258f,  1.2382417f, -0.9404192f,  2.6338796f,
		 1.3872274f,  1.4370065f, -2.2326224f, -4.0005569f,
		 2.8098016f, -0.2843922f, -3.1922000f, -0.5017270f,
		-3.2142730f,  0.9296157f,  3.3380206f, -0.3273013f,
		-2.0620086f,
		// Imaginary parts [17 .. 31] (negated from numpy values)
		 1.4368339f, -0.9718257f,  0.3860649f,  5.0175196f,
		 1.1372134f,  4.0448431f, -0.4110472f, -0.8750027f,
		 1.2779141f, -0.0101518f, -0.2213625f,  4.0391229f,
		 1.4086333f,  0.2290648f,  3.1862191f
	};

	float error = compareArrays(result.data(), expected.data(), FFT_LEN);
	CHECK(error < tolerance);
}

TEST_CASE("FFTReal: HalfZeroPadded mode")
{
	// Test that HalfZeroPadded mode (N/2 inputs) produces same result as Full mode
	// with second half explicitly zero-filled
	constexpr float tolerance = 1e-5f;
	constexpr unsigned FFT_LEN_L2 = 8;
	constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 256

	// First half: some arbitrary pattern
	std::array<float, FFT_LEN / 2> halfInput = {
		0.5f, -0.3f, 0.8f, -0.2f, 0.1f, 0.9f, -0.7f, 0.4f,
		0.2f, -0.6f, 0.3f, -0.1f, 0.7f, -0.5f, 0.6f, -0.4f,
		0.1f,  0.3f, -0.2f, 0.5f, -0.3f, 0.4f, -0.6f, 0.2f,
		-0.1f, 0.7f, -0.4f, 0.6f, -0.5f, 0.3f, -0.7f, 0.8f,
		0.4f, -0.2f, 0.9f, -0.1f, 0.5f, -0.8f, 0.3f, -0.6f,
		0.7f, -0.4f, 0.2f, -0.9f, 0.1f, -0.3f, 0.8f, -0.5f,
		-0.2f, 0.6f, -0.7f, 0.4f, -0.1f, 0.9f, -0.3f, 0.5f,
		0.8f, -0.6f, 0.2f, -0.4f, 0.7f, -0.9f, 0.3f, -0.1f,
		0.5f, -0.7f, 0.4f, -0.2f, 0.9f, -0.3f, 0.6f, -0.8f,
		0.1f, -0.5f, 0.3f, -0.6f, 0.8f, -0.2f, 0.7f, -0.4f,
		-0.3f, 0.9f, -0.1f, 0.4f, -0.7f, 0.2f, -0.5f, 0.6f,
		0.3f, -0.8f, 0.5f, -0.1f, 0.4f, -0.6f, 0.9f, -0.2f,
		0.7f, -0.4f, 0.1f, -0.9f, 0.3f, -0.5f, 0.8f, -0.6f,
		-0.2f, 0.4f, -0.7f, 0.9f, -0.3f, 0.6f, -0.1f, 0.5f,
		0.8f, -0.2f, 0.4f, -0.6f, 0.7f, -0.9f, 0.1f, -0.3f,
		0.5f, -0.8f, 0.6f, -0.4f, 0.2f, -0.7f, 0.9f, -0.1f
	};

	// Full input: same pattern + zeros
	std::array<float, FFT_LEN> fullInput = {};
	for (unsigned i = 0; i < FFT_LEN / 2; ++i) {
		fullInput[i] = halfInput[i];
	}

	std::array<float, FFT_LEN> outputFull, tmpBufFull;
	std::array<float, FFT_LEN> outputHalf, tmpBufHalf;

	auto resultFull = FFTReal<FFT_LEN_L2, FFTInputMode::Full>::execute(fullInput, outputFull, tmpBufFull);
	auto resultHalf = FFTReal<FFT_LEN_L2, FFTInputMode::HalfZeroPadded>::execute(halfInput, outputHalf, tmpBufHalf);

	float error = compareArrays(resultFull.data(), resultHalf.data(), FFT_LEN);
	CHECK(error < tolerance);
}

TEST_CASE("FFTReal: Symmetric mode")
{
	// Test that Symmetric mode (N/2+1 inputs) produces same result as Full mode
	// with mirrored second half. Output should be purely real (N/2+1 values).
	constexpr float tolerance = 1e-4f;
	constexpr unsigned FFT_LEN_L2 = 8;
	constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2; // 256
	constexpr unsigned OUTPUT_LEN = FFT_LEN / 2 + 1; // 129

	// Symmetric input (first half + center point)
	std::array<float, FFT_LEN / 2 + 1> symInput = {
		0.5f, -0.3f, 0.8f, -0.2f, 0.1f, 0.9f, -0.7f, 0.4f,
		0.2f, -0.6f, 0.3f, -0.1f, 0.7f, -0.5f, 0.6f, -0.4f,
		0.1f,  0.3f, -0.2f, 0.5f, -0.3f, 0.4f, -0.6f, 0.2f,
		-0.1f, 0.7f, -0.4f, 0.6f, -0.5f, 0.3f, -0.7f, 0.8f,
		0.4f, -0.2f, 0.9f, -0.1f, 0.5f, -0.8f, 0.3f, -0.6f,
		0.7f, -0.4f, 0.2f, -0.9f, 0.1f, -0.3f, 0.8f, -0.5f,
		-0.2f, 0.6f, -0.7f, 0.4f, -0.1f, 0.9f, -0.3f, 0.5f,
		0.8f, -0.6f, 0.2f, -0.4f, 0.7f, -0.9f, 0.3f, -0.1f,
		0.5f, -0.7f, 0.4f, -0.2f, 0.9f, -0.3f, 0.6f, -0.8f,
		0.1f, -0.5f, 0.3f, -0.6f, 0.8f, -0.2f, 0.7f, -0.4f,
		-0.3f, 0.9f, -0.1f, 0.4f, -0.7f, 0.2f, -0.5f, 0.6f,
		0.3f, -0.8f, 0.5f, -0.1f, 0.4f, -0.6f, 0.9f, -0.2f,
		0.7f, -0.4f, 0.1f, -0.9f, 0.3f, -0.5f, 0.8f, -0.6f,
		-0.2f, 0.4f, -0.7f, 0.9f, -0.3f, 0.6f, -0.1f, 0.5f,
		0.8f, -0.2f, 0.4f, -0.6f, 0.7f, -0.9f, 0.1f, -0.3f,
		0.5f, -0.8f, 0.6f, -0.4f, 0.2f, -0.7f, 0.9f, -0.1f,
		0.3f  // Center point (index 128)
	};

	// Full input: mirror to create symmetric array
	std::array<float, FFT_LEN> fullInput;
	for (unsigned i = 0; i <= FFT_LEN / 2; ++i) {
		fullInput[i] = symInput[i];
	}
	for (unsigned i = FFT_LEN / 2 + 1; i < FFT_LEN; ++i) {
		fullInput[i] = fullInput[FFT_LEN - i];
	}

	std::array<float, FFT_LEN> outputFull, tmpBufFull;
	std::array<float, FFT_LEN> outputSym, tmpBufSym;

	auto resultFull = FFTReal<FFT_LEN_L2, FFTInputMode::Full>::execute(fullInput, outputFull, tmpBufFull);
	auto resultSym  = FFTReal<FFT_LEN_L2, FFTInputMode::Symmetric>::execute(symInput, outputSym, tmpBufSym);

	float error = compareArrays(resultFull.data(), resultSym.data(), OUTPUT_LEN);
	CHECK(error < tolerance);
}
