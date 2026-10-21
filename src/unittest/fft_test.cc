#include "catch.hpp"
#include "fft.hh"
#include "FFTReference.hh"

#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <vector>

// Keep fft::FFT<> instantiations few: each builds large constexpr tables and
// (under -O3 -fsanitize=address) a lot of optimized code. Coverage strategy:
//   * L2=8: all input modes + Power where the code path differs
//   * L2=9 Full only: odd default plan (final radix-8)
//   * One alternate radix plan
//   * Known-pattern / layout checks at L2=8
//   * Autocorrelation pipeline (the openMSX use case)

static constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
static constexpr double tolerance = 2e-6; // measured error ~1e-7; bugs are O(1)

TEST_CASE("fft: reference FFT sanity check")
{
	using namespace fft_reference;
	for (unsigned N : {2u, 4u, 16u, 64u}) {
		auto in = randomSignal(N, N);
		auto x = expandInput(fft::InputMode::Full, in, N);
		std::vector<std::complex<double>> X(x.begin(), x.end());
		complexFFT(X);
		auto slow = slowDFT(x);
		for (unsigned k = 0; k < N; ++k) {
			CHECK(std::abs(X[k] - slow[k]) < 1e-12);
		}
	}
}

static float compareArrays(const float* a, const float* b, size_t size)
{
	float maxError = 0.0f;
	for (size_t i = 0; i < size; ++i) {
		maxError = std::max(maxError, std::abs(a[i] - b[i]));
	}
	return maxError;
}

template<typename FFT, fft::InputMode MODE, bool POWER>
static double fftError(uint32_t seed)
{
	constexpr unsigned N = FFT::FFT_LEN;
	constexpr unsigned W = FFT::WORK_LEN;
	auto in = fft_reference::randomSignal(FFT::INPUT_LEN, seed);
	std::vector<double> ref;
	if constexpr (MODE == fft::InputMode::Complex) {
		ref = fft_reference::referenceComplexFFT(in);
		if constexpr (POWER) {
			std::vector<double> p(N);
			for (unsigned k = 0; k < N; ++k) {
				p[k] = ref[k] * ref[k] + ref[N + k] * ref[N + k];
			}
			ref = std::move(p);
		}
	} else {
		ref = fft_reference::referenceFFT(MODE, in, N);
		if constexpr (POWER) ref = fft_reference::powerSpectrum(ref);
	}
	std::vector<float> output(W, NaN), tmpBuf(W, NaN);
	auto result = FFT::execute(std::span<const float, FFT::INPUT_LEN>(in),
	                           std::span<float, W>(output), std::span<float, W>(tmpBuf));
	CHECK(result.data() == output.data());
	return fft_reference::relativeError(result, ref, FFT::OUTPUT_LEN);
}

template<unsigned L2, fft::InputMode MODE, fft::OutputMode OUT, unsigned... RADICES_L2>
static void checkRef(uint32_t seed = 1)
{
	constexpr bool POWER = OUT == fft::OutputMode::Power;
	INFO("L2=" << L2 << " MODE=" << int(MODE) << " OUT=" << int(OUT));
	CHECK(fftError<fft::FFT<L2, MODE, OUT, RADICES_L2...>, MODE, POWER>(seed) < tolerance);
}

TEST_CASE("fft: Full mode - known patterns")
{
	constexpr float tol = 1e-4f;
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	std::array<float, N> input{}, output, tmpBuf;

	SECTION("All zeros") {
		auto result = fft::FFT<L2>::execute(input, output, tmpBuf);
		for (unsigned i = 0; i < N; ++i) CHECK(std::abs(result[i]) < tol);
	}
	SECTION("Impulse at t=0") {
		input[0] = 1.0f;
		auto result = fft::FFT<L2>::execute(input, output, tmpBuf);
		for (unsigned i = 0; i <= N / 2; ++i) CHECK(std::abs(result[i] - 1.0f) < tol);
		for (unsigned i = N / 2 + 1; i < N; ++i) CHECK(std::abs(result[i]) < tol);
	}
	SECTION("Impulse at t=1") {
		// X[k] = e^{-2πik/N} = cos - i·sin → packed Im half is -sin.
		input[1] = 1.0f;
		auto result = fft::FFT<L2>::execute(input, output, tmpBuf);
		for (unsigned k = 0; k <= N / 2; ++k) {
			CHECK(std::abs(result[k] - float(std::cos(2.0 * std::numbers::pi * k / N))) < tol);
		}
		for (unsigned k = 1; k < N / 2; ++k) {
			CHECK(std::abs(result[N / 2 + k] + float(std::sin(2.0 * std::numbers::pi * k / N))) < tol);
		}
	}
	SECTION("Constant / Nyquist") {
		input.fill(1.0f);
		auto result = fft::FFT<L2>::execute(input, output, tmpBuf);
		CHECK(std::abs(result[0] - float(N)) < tol);
		for (unsigned i = 1; i < N; ++i) CHECK(std::abs(result[i]) < tol);

		for (unsigned i = 0; i < N; ++i) input[i] = (i & 1) ? -1.0f : 1.0f;
		result = fft::FFT<L2>::execute(input, output, tmpBuf);
		for (unsigned i = 0; i < N; ++i) {
			float expected = (i == N / 2) ? float(N) : 0.0f;
			CHECK(std::abs(result[i] - expected) < tol);
		}
	}
}

TEST_CASE("fft: single tones (output layout)")
{
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	constexpr float tol = 2e-4f;
	for (unsigned k : {1u, N / 4, N / 2 - 1}) {
		for (bool sine : {false, true}) {
			INFO("k=" << k << " sine=" << sine);
			std::array<float, N> input, output, tmpBuf;
			for (unsigned n = 0; n < N; ++n) {
				double phase = 2.0 * std::numbers::pi * double((k * n) % N) / N;
				input[n] = float(sine ? std::sin(phase) : std::cos(phase));
			}
			auto result = fft::FFT<L2>::execute(input, output, tmpBuf);
			// Cosine → +N/2 at bin k; sine → Im X[k] = -N/2 at packed slot N/2+k.
			for (unsigned i = 0; i < N; ++i) {
				float expected = 0.0f;
				if (!sine && i == k) expected = float(N / 2);
				if (sine && i == N / 2 + k) expected = -float(N / 2);
				CHECK(std::abs(result[i] - expected) < tol);
			}
		}
	}
}

TEST_CASE("fft: HalfZeroPadded and Symmetric vs Full")
{
	constexpr float tol = 1e-4f;
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	auto signal = fft_reference::randomSignal(N / 2 + 1, 42);

	SECTION("HalfZeroPadded") {
		std::array<float, N / 2> halfInput;
		std::array<float, N> fullInput{};
		for (unsigned i = 0; i < N / 2; ++i) halfInput[i] = fullInput[i] = signal[i];
		std::array<float, N> outF, tmpF, outH, tmpH;
		auto full = fft::FFT<L2, fft::InputMode::Full>::execute(fullInput, outF, tmpF);
		auto half = fft::FFT<L2, fft::InputMode::HalfZeroPadded>::execute(halfInput, outH, tmpH);
		CHECK(compareArrays(full.data(), half.data(), N) < tol);
	}
	SECTION("Symmetric") {
		std::array<float, N / 2 + 1> symInput;
		std::array<float, N> fullInput;
		for (unsigned i = 0; i <= N / 2; ++i) symInput[i] = fullInput[i] = signal[i];
		for (unsigned i = N / 2 + 1; i < N; ++i) fullInput[i] = fullInput[N - i];
		std::array<float, N> outF, tmpF, outS, tmpS;
		auto full = fft::FFT<L2, fft::InputMode::Full>::execute(fullInput, outF, tmpF);
		auto sym  = fft::FFT<L2, fft::InputMode::Symmetric>::execute(symInput, outS, tmpS);
		CHECK(compareArrays(full.data(), sym.data(), N / 2 + 1) < tol);
		for (unsigned i = N / 2 + 1; i < N; ++i) CHECK(std::abs(full[i]) < tol);
	}
}

TEST_CASE("fft: modes vs reference")
{
	// Minimum sizes: real L2=4 (M=8, leading radix-8 only), complex L2=4
	// (M=16, radix-8 + trailing radix-2). These exercise the scalar
	// first-pass / short unpack paths (M < 64).
	checkRef<4, fft::InputMode::Full,           fft::OutputMode::Complex>();
	checkRef<4, fft::InputMode::HalfZeroPadded, fft::OutputMode::Complex>();
	checkRef<4, fft::InputMode::Symmetric,      fft::OutputMode::Complex>();
	checkRef<4, fft::InputMode::Complex,        fft::OutputMode::Complex>();
	checkRef<4, fft::InputMode::Full,           fft::OutputMode::Power>();
	checkRef<4, fft::InputMode::Complex,        fft::OutputMode::Power>();
	// Trailing radix-2 plan (M_L2 = 4) for real input.
	checkRef<5, fft::InputMode::Full,           fft::OutputMode::Complex>();
	// Boundary of the 8-wide first pass (M = 64): real L2=7 / complex L2=6.
	checkRef<7, fft::InputMode::Full,           fft::OutputMode::Complex>();
	checkRef<6, fft::InputMode::Complex,        fft::OutputMode::Complex>();
	// Odd default plan (final radix-8).
	checkRef<9, fft::InputMode::Full,           fft::OutputMode::Complex>();
}

TEST_CASE("fft: alternate radix plan")
{
	// Remaining radices after the leading radix-8 sum to L2-4 = 4.
	checkRef<8, fft::InputMode::Full, fft::OutputMode::Complex, 1, 3>();
}

TEST_CASE("fft: Power output - known patterns")
{
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	constexpr unsigned M = N / 2;
	using FFT = fft::FFT<L2, fft::InputMode::Full, fft::OutputMode::Power>;
	std::array<float, N> input, output, tmpBuf;

	auto checkResult = [&](std::span<const float, M + 1> result, auto expected, float peak) {
		for (unsigned i = 0; i <= M; ++i) {
			INFO("i = " << i);
			CHECK(std::abs(result[i] - expected(i)) < 1e-5f * peak);
		}
	};

	SECTION("Impulse") {
		input.fill(0.0f);
		input[3] = 1.0f;
		checkResult(FFT::execute(input, output, tmpBuf), [](unsigned) { return 1.0f; }, 1.0f);
	}
	SECTION("Constant") {
		input.fill(1.0f);
		constexpr float peak = float(N) * float(N);
		checkResult(FFT::execute(input, output, tmpBuf),
		            [&](unsigned i) { return (i == 0) ? peak : 0.0f; }, peak);
	}
	SECTION("Single tone") {
		constexpr unsigned k0 = 5;
		constexpr float peak = float(M) * float(M);
		for (unsigned n = 0; n < N; ++n) {
			input[n] = float(std::cos(2.0 * std::numbers::pi * double((k0 * n) % N) / N));
		}
		checkResult(FFT::execute(input, output, tmpBuf),
		            [&](unsigned i) { return (i == k0) ? peak : 0.0f; }, peak);
	}
}

TEST_CASE("fft: autocorrelation via Power output")
{
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	constexpr unsigned S = N / 2;
	auto signal = fft_reference::randomSignal(S, 5);
	std::vector<float> spectrum(N), result(N), work(N);
	auto power = fft::FFT<L2, fft::InputMode::HalfZeroPadded, fft::OutputMode::Power>::execute(
		std::span<const float, S>(signal),
		std::span<float, N>(spectrum), std::span<float, N>(work));
	auto autocorr = fft::FFT<L2, fft::InputMode::Symmetric>::execute(
		power, std::span<float, N>(result), std::span<float, N>(work));

	std::vector<double> expected(S);
	for (unsigned t = 0; t < S; ++t) {
		double sum = 0.0;
		for (unsigned n = 0; n + t < S; ++n) sum += double(signal[n]) * double(signal[n + t]);
		expected[t] = sum * N;
	}
	CHECK(fft_reference::relativeError(autocorr, expected, S) < tolerance);
}

TEST_CASE("fft: Complex input - known patterns")
{
	constexpr unsigned L2 = 8;
	constexpr unsigned N = 1 << L2;
	using FFT = fft::FFT<L2, fft::InputMode::Complex>;
	std::array<float, 2 * N> input{}, output, tmpBuf;
	constexpr float tol = 1e-4f;

	SECTION("Impulse at n=0") {
		input[0] = 1.0f;
		auto result = FFT::execute(input, output, tmpBuf);
		for (unsigned k = 0; k < N; ++k) {
			CHECK(std::abs(result[k] - 1.0f) < tol);
			CHECK(std::abs(result[N + k]) < tol);
		}
	}
	SECTION("Single frequency bin") {
		constexpr unsigned k0 = 5;
		for (unsigned n = 0; n < N; ++n) {
			double phase = 2.0 * std::numbers::pi * double((k0 * n) % N) / N;
			input[2 * n + 0] = float(std::cos(phase));
			input[2 * n + 1] = float(std::sin(phase));
		}
		auto result = FFT::execute(input, output, tmpBuf);
		for (unsigned k = 0; k < N; ++k) {
			float eRe = (k == k0) ? float(N) : 0.0f;
			CHECK(std::abs(result[k] - eRe) < 2e-3f);
			CHECK(std::abs(result[N + k]) < 2e-3f);
		}
	}
}
