#include "catch.hpp"
#include "utils/autocorrelation.hh"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numbers>
#include <span>
#include <vector>

// Reference implementation (O(N²), but always correct)
static std::vector<float> autocorrReference(const std::vector<float>& signal)
{
	const size_t N = signal.size();
	std::vector<float> result(N);

	for (size_t lag = 0; lag < N; ++lag) {
		float sum = 0.0f;
		for (size_t i = 0; i < N - lag; ++i) {
			sum += signal[i] * signal[i + lag];
		}
		result[lag] = sum;
	}

	return result;
}

// Helper: compute max error between two arrays
static float maxError(const std::vector<float>& a, const std::vector<float>& b)
{
	assert(a.size() == b.size());
	float maxErr = 0.0f;
	for (size_t i = 0; i < a.size(); ++i) {
		maxErr = std::max(maxErr, std::abs(a[i] - b[i]));
	}
	return maxErr;
}

TEST_CASE("autocorrelation: zero signal")
{
	constexpr unsigned FFT_L2 = 5;
	constexpr unsigned N = 1 << (FFT_L2 - 1);

	std::vector<float> signal(N, 0.0f); // All zeros

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// Autocorrelation of all zeros should be all zeros
	for (unsigned i = 0; i < N; ++i) {
		CHECK(std::abs(autocorr[i]) < 1e-5f);
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-5f);
}

TEST_CASE("autocorrelation: impulse signal")
{
	constexpr unsigned FFT_L2 = 5;
	constexpr unsigned N = 1 << (FFT_L2 - 1);

	std::vector<float> signal(N, 0.0f);
	signal[0] = 1.0f; // Impulse

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// Verify impulse response: autocorr[0] = 1, all others = 0
	CHECK(std::abs(autocorr[0] - 1.0f) < 1e-4f);
	for (unsigned i = 1; i < N; ++i) {
		CHECK(std::abs(autocorr[i]) < 1e-4f);
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-4f);
}

TEST_CASE("autocorrelation: constant signal")
{
	constexpr unsigned FFT_L2 = 5;
	constexpr unsigned N = 1 << (FFT_L2 - 1);

	std::vector<float> signal(N, 2.0f); // All values = 2.0

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// For constant signal, autocorr[τ] = (N-τ) * value²
	constexpr float value = 2.0f;
	for (unsigned tau = 0; tau < N; ++tau) {
		float expected = (N - tau) * value * value;
		CHECK(std::abs(autocorr[tau] - expected) < 1e-3f);
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-3f); // Slightly higher tolerance for larger values
}

TEST_CASE("autocorrelation: sine wave")
{
	constexpr unsigned FFT_L2 = 9;
	constexpr unsigned N = 1 << (FFT_L2 - 1);
	constexpr float freq = 5.0f; // 5 complete cycles

	std::vector<float> signal(N);
	for (unsigned i = 0; i < N; ++i) {
		signal[i] = std::sin(2.0f * std::numbers::pi_v<float> * freq * i / N);
	}

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// Verify autocorr[0] is maximum
	for (unsigned i = 1; i < N; ++i) {
		CHECK(autocorr[i] <= autocorr[0] + 1e-4f); // Allow small numerical error
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-3f);
}

TEST_CASE("autocorrelation: random signal")
{
	constexpr unsigned FFT_L2 = 8;
	constexpr unsigned N = 1 << (FFT_L2 - 1);

	// Use fixed seed for deterministic results
	srand(42);
	std::vector<float> signal(N);
	for (unsigned i = 0; i < N; ++i) {
		signal[i] = static_cast<float>(rand()) / RAND_MAX - 0.5f;
	}

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// Verify autocorr[0] is maximum
	for (unsigned i = 1; i < N; ++i) {
		CHECK(autocorr[i] <= autocorr[0] + 1e-4f);
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-3f);
}

TEST_CASE("autocorrelation: non-power-of-2 signal length")
{
	constexpr unsigned FFT_L2 = 8;
	constexpr unsigned N = 20;  // Not a power of 2 (FFT_LEN = 256 would allow up to 128)

	std::vector<float> signal(N);
	for (unsigned i = 0; i < N; ++i) {
		signal[i] = static_cast<float>(i + 1) * 0.1f;  // Simple increasing sequence
	}

	std::vector<float> autocorr(N);
	autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
	auto reference = autocorrReference(signal);

	// Verify autocorr[0] is maximum
	for (unsigned i = 1; i < N; ++i) {
		CHECK(autocorr[i] <= autocorr[0] + 1e-4f);
	}

	// Verify against reference implementation
	float error = maxError(autocorr, reference);
	CHECK(error < 1e-3f);
}

TEST_CASE("autocorrelation: fewer lags than samples")
{
	// The YIN use case: N samples, only L lags, with N + L <= FFT_LEN + 1
	// (so FFT_LEN can be smaller than 2N).
	constexpr unsigned FFT_L2 = 8;
	constexpr unsigned FFT_LEN = 1 << FFT_L2;
	for (unsigned N : {129u, 200u, 225u, 256u}) {
		const unsigned L = std::min(FFT_LEN + 1 - N, FFT_LEN / 2);
		std::vector<float> signal(N);
		srand(N);
		for (auto& x : signal) x = float(rand()) / float(RAND_MAX) - 0.5f;

		std::vector<float> autocorr(L);
		autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
		auto reference = autocorrReference(signal);
		reference.resize(L);

		INFO("N=" << N << " L=" << L);
		float error = maxError(autocorr, reference);
		CHECK(error < 1e-3f);
	}
}

TEST_CASE("autocorrelation: small FFT sizes")
{
	// FFT_LEN 32 and 64 use the scalar (partial block) first pass, 128 is
	// the first full block; FFT_LEN 32 uses the short middle loop. Also
	// covers odd and even signal lengths.
	auto check = [&]<unsigned FFT_L2>() {
		constexpr unsigned FFT_LEN = 1 << FFT_L2;
		for (unsigned N : {FFT_LEN / 2, FFT_LEN / 2 - 1, FFT_LEN / 4 + 1}) {
			std::vector<float> signal(N);
			srand(N);
			for (auto& x : signal) x = float(rand()) / float(RAND_MAX) - 0.5f;
			std::vector<float> autocorr(N);
			autocorrelation<FFT_L2>(std::span(signal), std::span(autocorr));
			auto reference = autocorrReference(signal);
			INFO("FFT_LEN=" << FFT_LEN << " N=" << N);
			CHECK(maxError(autocorr, reference) < 1e-4f);
		}
	};
	check.operator()<5>();
	check.operator()<6>();
	check.operator()<7>();
}

TEST_CASE("autocorrelation: normalization")
{
	constexpr unsigned FFT_L2 = 8;
	constexpr unsigned N = 1 << (FFT_L2 - 1);

	std::vector<float> signal(N);
	for (unsigned i = 0; i < N; ++i) {
		signal[i] = static_cast<float>(i + 1) * 0.1f;
	}

	std::vector<float> autocorrNormalized(N);
	std::vector<float> autocorrUnnormalized(N);

	// Test with normalization (default)
	autocorrelation<FFT_L2, true>(std::span(signal), std::span(autocorrNormalized));

	// Test without normalization
	autocorrelation<FFT_L2, false>(std::span(signal), std::span(autocorrUnnormalized));

	// Unnormalized should be FFT_LEN times larger (up to float rounding:
	// the scale is applied at a different point in the computation)
	constexpr unsigned FFT_LEN = 1 << FFT_L2;
	const float tolerance = 1e-6f * autocorrNormalized[0];
	for (unsigned i = 0; i < N; ++i) {
		float expected = autocorrUnnormalized[i] / FFT_LEN;
		CHECK(std::abs(autocorrNormalized[i] - expected) < tolerance);
	}
}
