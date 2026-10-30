// YIN pitch detection algorithm
// Based on: de Cheveigné & Kawahara (2002)
// "YIN, a fundamental frequency estimator for speech and music"

#include "autocorrelation.hh"
#include "narrow.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <span>

namespace yin {

template<typename T> [[nodiscard]] constexpr T square(T t)
{
	return t * t;
}

// Helper: Compute difference function d(τ) using autocorrelation
//
// d(τ) = Σ(x[n] - x[n+τ])²
//      = Σx[n]² + Σx[n+τ]² - 2·Σx[n]·x[n+τ]
//      = energyLeft(τ) + energyRight(τ) - 2·r(τ)
//   energyLeft(τ)  = Σ x[n]² for n in [0, N-τ) = total - Σ x[n]² for n in [N-τ, N)
//   energyRight(τ) = Σ x[n]² for n in [τ, N)   = total - Σ x[n]² for n in [0, τ)
//
// Returns: Total energy of the signal (sum of all squared samples)
//
// Preconditions:
//   - autocorr.size() == diff.size()
//   - diff.size() <= signal.size() / 2 + 1
//   - WORK_LEN >= diff.size() (for internal buffers)
template<size_t WORK_LEN>
[[nodiscard]] inline float computeDifferenceFunction(
	std::span<const float> signal,
	std::span<const float> autocorr,
	std::span<float> diff)
{
	assert(autocorr.size() == diff.size());
	assert(diff.size() <= signal.size() / 2 + 1);
	const auto N = signal.size();
	const auto maxLag = diff.size();
	assert(WORK_LEN >= maxLag);

	// Total energy Σ x[n]² is the autocorrelation at lag 0.
	const auto totalEnergy = autocorr[0];

	// Running sums of x[n]² over the first and the last maxLag samples:
	//   head[τ] = Σ x[n]² for n in [0, τ)   = total - energyRight(τ)
	//   tail[τ] = Σ x[n]² for n in [N-τ, N) = total - energyLeft(τ)
	// Only these two windows are needed, not a prefix sum over all N samples
	// (which is a long serial dependency chain).
	std::array<float, WORK_LEN> head; // ok, uninitialized
	std::array<float, WORK_LEN> tail; // ok, uninitialized
	head[0] = tail[0] = 0.0f;
	for (auto lag : xrange(size_t(1), maxLag)) {
		head[lag] = head[lag - 1] + square(signal[lag - 1]);
		tail[lag] = tail[lag - 1] + square(signal[N - lag]);
	}

	// d(τ) = energyLeft + energyRight - 2·r(τ) = 2·total - tail[τ] - head[τ] - 2·r(τ)
	diff[0] = 0.0f; // Perfect match at lag 0
	for (auto lag : xrange(size_t(1), maxLag)) {
		diff[lag] = 2.0f * (totalEnergy - autocorr[lag]) - tail[lag] - head[lag];
	}

	return totalEnergy;
}

// Helper: Cumulative mean normalization
//
// d'(τ) = d(τ) / [(1/τ) · Σd(j) for j=1..τ]
//       = d(τ) · τ / Σd(j) for j=1..τ
//
// This is the key innovation of YIN that reduces octave errors.
inline void cumulativeMeanNormalize(std::span<const float> diff,
                                    std::span<float> diffNorm)
{
	assert(diff.size() == diffNorm.size());

	// First the running sum (inherently serial) into diffNorm, then the
	// division in a separate loop that the compiler can vectorize.
	diffNorm[0] = 1.0f; // Special case
	auto runningSum = 0.0f;
	for (auto lag : xrange(size_t(1), diff.size())) {
		runningSum += diff[lag];
		diffNorm[lag] = runningSum;
	}
	for (auto lag : xrange(size_t(1), diff.size())) {
		auto sum = diffNorm[lag];
		diffNorm[lag] = (sum > 0.0f) ? (diff[lag] * float(lag) / sum) : 1.0f;
	}
}

// Helper: Find first minimum below threshold
//
// Returns the lag corresponding to the fundamental period.
// Uses YIN's absolute threshold approach.
// Always returns a result (the best candidate found, even if weak).
//
// Preconditions:
//   - minLag < maxLag
//   - diffNorm.size() >= maxLag (typically size == maxLag + 1)
[[nodiscard]] inline size_t findBestLag(
	std::span<const float> diffNorm, size_t minLag,
	size_t maxLag, float threshold)
{
	assert(minLag < maxLag);
	assert(diffNorm.size() >= maxLag);

	// Single-pass: look for first value below threshold (with early exit)
	// while tracking global minimum as fallback
	auto bestLag = minLag;
	auto minVal = diffNorm[minLag];

	for (auto lag : xrange(minLag, maxLag)) {
		auto val = diffNorm[lag];

		// Check for first value below threshold -> early exit
		if (val < threshold) {
			// Found candidate, search forward for local minimum
			while (lag + 1 < maxLag &&
			       diffNorm[lag + 1] < diffNorm[lag]) {
				++lag;
			}
			return lag; // Early exit!
		}

		// Track global minimum (used if nothing below threshold)
		if (val < minVal) {
			minVal = val;
			bestLag = lag;
		}
	}

	// No value below threshold, use tracked global minimum
	// (Return even if confidence is low - let caller decide if acceptable)
	return bestLag;
}

// Helper: Parabolic interpolation for sub-sample accuracy
//
// Uses 3 points around the minimum to fit a parabola and find the true minimum.
[[nodiscard]] inline float parabolicInterpolation(
	std::span<const float> diffNorm, size_t lag)
{
	// Need neighbors on both sides (check array bounds only)
	if (lag == 0 || lag >= diffNorm.size() - 1) {
		return float(lag);
	}

	auto y1 = diffNorm[lag - 1];
	auto y2 = diffNorm[lag + 0];
	auto y3 = diffNorm[lag + 1];

	// Check we have a proper minimum (not maximum or flat)
	auto denom = y1 - 2.0f * y2 + y3;
	if (std::abs(denom) <= 1e-10f || y1 <= y2 || y3 <= y2) {
		// Can't fit parabola or not a minimum
		return float(lag);
	}

	// Parabolic interpolation formula
	// The offset is mathematically guaranteed to be in (-1, 1) given the
	// guards above: For a concave-up parabola where the middle of three
	// equally-spaced points is strictly lower than both neighbors, the
	// vertex must lie between them.
	auto offset = 0.5f * (y1 - y3) / denom;
	return float(lag) + offset;
}

struct PitchResult {
	float frequency = 0.0f; // Detected frequency in Hz (0.0 = no pitch detected)
	float error = 1.0f; // YIN aperiodicity measure (0.0 = perfect periodic,
	                    // higher = worse) Typical: <0.1 excellent, 0.1-0.2
	                    // good, 0.2-0.5 weak, >0.5 poor Default 1.0 = no
	                    // valid pitch detected
};

// Template helper that does the actual YIN computation for a specific FFT size
template<unsigned FFT_LEN_L2>
[[nodiscard]] inline PitchResult detectPitchImpl(
	std::span<const float> signal, float sampleRate,
	size_t minLag, size_t maxLag, float threshold)
{
	static constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2;
	// Buffer size, given N + maxLag <= FFT_LEN (see detectPitch()) and
	// 1 <= maxLag <= N/2: maxLag + 1 <= FFT_LEN / 2.
	static constexpr unsigned LAG_LEN = FFT_LEN / 2;
	assert(signal.size() + maxLag <= FFT_LEN);
	assert(minLag < maxLag);
	if (maxLag >= LAG_LEN || minLag >= maxLag) [[unlikely]] {
		// Cannot happen (see detectPitch()), but makes the index ranges
		// below provably in bounds, also for the compiler.
		return {};
	}

	// Autocorrelation, only for the lags 0..maxLag that YIN needs
	std::array<float, LAG_LEN> autocorr_; // ok, uninitialized
	auto autocorr = std::span(autocorr_).first(maxLag + 1);
	autocorrelation<FFT_LEN_L2>(signal, autocorr);

	// Compute difference function
	std::array<float, LAG_LEN> diff_; // ok, uninitialized
	auto diff = std::span(diff_).first(maxLag + 1);
	auto totalEnergy = computeDifferenceFunction<LAG_LEN>(signal, autocorr, diff);

	// Early exit if signal is essentially silence
	if (totalEnergy < 1e-10f) return {};

	// Cumulative mean normalization
	std::array<float, LAG_LEN> diffNorm_; // ok, uninitialized
	auto diffNorm = std::span(diffNorm_).first(maxLag + 1);
	cumulativeMeanNormalize(diff, diffNorm);

	// Find best lag (always returns a result, even if confidence is low)
	auto bestLag = findBestLag(diffNorm, minLag, maxLag, threshold);

	// Parabolic interpolation
	auto refinedLag = parabolicInterpolation(diffNorm, bestLag);

	// Convert to frequency and get error measure
	auto frequency = sampleRate / refinedLag;
	auto error = std::max(diffNorm[bestLag], 0.0f); // YIN normalized difference (lower is better)
	return {frequency, error};
}

// Main YIN pitch detection function (public API)
//
// Detects the fundamental frequency of an audio signal using the YIN algorithm.
//
// Parameters:
//   signal     - Audio samples
//   sampleRate - Sample rate in Hz (e.g., 44100.0f)
//   minFreq    - Minimum frequency to search for (default: 50 Hz)
//   maxFreq    - Maximum frequency to search for (default: 2000 Hz)
//   threshold  - YIN threshold parameter (default: 0.15, range: 0.1-0.2)
//
// Returns:
//   PitchResult with:
//     frequency  - Detected pitch in Hz
//     error      - YIN aperiodicity measure (lower is better)
//                  Typical values: <0.1 = excellent, 0.1-0.2 = good, 0.2-0.5 =
//                  weak, >0.5 = poor Caller should filter out results with high
//                  error (e.g., >0.5) Can exceed 1.0 for very non-periodic
//                  signals (drums, noise)
//
// Preconditions (checked by assertions in debug builds):
//   - signal.size() >= 2
//   - minFreq < maxFreq
//   - signal.size() >= 2 * sampleRate / minFreq (approximately)
//     i.e., signal must contain at least ~2 periods of the lowest frequency
//   - Recommended buffer size: ~2048 samples @ 44.1kHz for good frequency
//   resolution
//     (allows detecting down to ~43 Hz with 2048 samples @ 44.1kHz)
[[nodiscard]] inline PitchResult detectPitch(
	std::span<const float> signal,
	float sampleRate,
	float minFreq = 50.0f,
	float maxFreq = 2000.0f,
	float threshold = 0.15f)
{
	const auto N = signal.size();

	// Calculate lag range
	const auto minLag = std::max(size_t(1), size_t(sampleRate / maxFreq));
	const auto maxLag = std::min(size_t(sampleRate / minFreq), N / 2);

	// Preconditions: signal must be large enough and frequency range must
	// be valid
	assert(N >= 2);
	assert(maxLag > minLag);

	// Determine FFT size: the autocorrelation via FFT is circular, so lags
	// up to maxLag are only correct when N + maxLag <= FFT_LEN. Since
	// maxLag <= N/2 this is at most 1.5 N, typically one power of 2 below
	// the 2N that would be needed for all lags. Very short signals use the
	// smallest supported FFT length (32).
	const auto fftLenL2 = std::max(5u, narrow<unsigned>(std::bit_width(N + maxLag - 1)));

	// Dispatch to appropriate FFT size
	switch (fftLenL2) {
	case  5: return detectPitchImpl< 5>(signal, sampleRate, minLag, maxLag, threshold);
	case  6: return detectPitchImpl< 6>(signal, sampleRate, minLag, maxLag, threshold);
	case  7: return detectPitchImpl< 7>(signal, sampleRate, minLag, maxLag, threshold);
	case  8: return detectPitchImpl< 8>(signal, sampleRate, minLag, maxLag, threshold);
	case  9: return detectPitchImpl< 9>(signal, sampleRate, minLag, maxLag, threshold);
	case 10: return detectPitchImpl<10>(signal, sampleRate, minLag, maxLag, threshold);
	case 11: return detectPitchImpl<11>(signal, sampleRate, minLag, maxLag, threshold);
	case 12: return detectPitchImpl<12>(signal, sampleRate, minLag, maxLag, threshold);
	case 13: return detectPitchImpl<13>(signal, sampleRate, minLag, maxLag, threshold);
	case 14: return detectPitchImpl<14>(signal, sampleRate, minLag, maxLag, threshold);
	default:
		// Unsupported FFT size - signal too large or too small
		assert(false && "Unsupported FFT size in detectPitchYIN");
		return {};
	}
}

} // namespace yin
