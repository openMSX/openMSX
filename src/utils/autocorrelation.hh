#ifndef AUTOCORRELATION_HH
#define AUTOCORRELATION_HH

#include "FFTReal.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

// Compute autocorrelation of a real signal:
//     autocorr[τ] = Σ signal[n] · signal[n+τ]
//
// Template parameters:
//   SIGNAL_LEN_L2 = log2(maximum allowed signal length).
//     This is a hard upper bound: callers must ensure `signal.size() <= pow(2, SIGNAL_LEN_L2)`.
//     For efficiency pick the smallest value that fits your input, i.e.
//     `SIGNAL_LEN_L2 = ceil(log2(signal.size()))`.
//   NORMALIZE = whether to normalize result by 1/FFT_LEN (default: true)
//
// Parameters:
//   signal: Input signal (length <= pow(2, SIGNAL_LEN_L2))
//   output: Autocorrelation result (same length as signal)
//
// Output properties:
//   output[0] = maximum value (signal energy)
//   output[τ] = autocorrelation at lag τ
//
// Algorithm (Wiener-Khinchin theorem):
//   1. Zero-pad signal to 2*signal_len for linear (not circular) correlation
//   2. Forward Real FFT: signal → spectrum (complex, Hermitian symmetric)
//   3. Power spectrum: power[k] = |spectrum[k]|² (real, even symmetric)
//   4. Inverse Real FFT: power → autocorrelation (via FFT of even-symmetric input)
template<unsigned SIGNAL_LEN_L2, bool NORMALIZE = true>
void autocorrelation(std::span<const float> signal, std::span<float> output)
{
	static_assert(SIGNAL_LEN_L2 >= 2);  // max signal length at least 4
	static_assert(SIGNAL_LEN_L2 <= 15); // max signal length at most 32768
	static constexpr unsigned SIGNAL_LEN = 1 << SIGNAL_LEN_L2; // Maximum signal length
	static constexpr unsigned FFT_LEN = 2 * SIGNAL_LEN; // For linear correlation padding

	const auto N = signal.size();
	assert(N <= SIGNAL_LEN);
	assert(N == output.size());

	// Step 1: Zero-pad signal to FFT_LEN. This serves two purposes:
	// 1. Extend from N to 2*N (required for linear autocorrelation, not circular)
	// 2. Round up to power-of-2 (required by FFT)
	std::array<float, FFT_LEN> paddedSignal;  // Zero-padded input signal
	std::ranges::copy(signal, paddedSignal.begin());
	std::ranges::fill(std::span(paddedSignal).subspan(N), 0.0f);

	// Step 2: Forward Real FFT
	// paddedSignal (time) → spectrum (frequency)
	std::array<float, FFT_LEN> spectrum; // FFT spectrum / power spectrum
	std::array<float, FFT_LEN> fftWork;  // Temporary work buffer for FFT operations
	FFTReal<SIGNAL_LEN_L2 + 1>::execute(paddedSignal, spectrum, fftWork);
	// spectrum now contains FFT in packed format:
	//   [Re[0], Re[1], ..., Re[N/2], Im[1], ..., Im[N/2-1]]

	// Step 3: Compute power spectrum: |FFT|² = Re² + Im² (in-place)
	// DC component (k=0): purely real, no imaginary part
	spectrum[0] = spectrum[0] * spectrum[0];
	// Nyquist component (k=FFT_LEN/2): purely real, no imaginary part
	spectrum[SIGNAL_LEN] = spectrum[SIGNAL_LEN] * spectrum[SIGNAL_LEN];
	// Other bins (k=1 to SIGNAL_LEN-1): complex values
	// Real parts: spectrum[1..SIGNAL_LEN-1]
	// Imaginary parts: spectrum[SIGNAL_LEN+1..FFT_LEN-1]
	for (unsigned i = 1; i < SIGNAL_LEN; ++i) {
		const float re = spectrum[i];
		const float im = spectrum[SIGNAL_LEN + i];
		spectrum[i] = re * re + im * im; // |spectrum[i]|²
	}
	// spectrum now contains: [P[0], P[1], ..., P[N/2], <stuff that will be overwritten>]
	// where P[k] = |spectrum[k]|² (real-valued power spectrum)

	// Step 3.1: Mirror the power spectrum to create even symmetry
	// We need: [P[0], P[1], ..., P[SIGNAL_LEN], P[SIGNAL_LEN-1], ..., P[1]]
	// using even symmetry: P[-k] = P[k] = P[N-k]
	for (unsigned i = 1; i < SIGNAL_LEN; ++i) {
		spectrum[FFT_LEN - i] = spectrum[i]; // P[N-k] = P[k]
	}
	// spectrum is now: a complete real + even symmetric signal

	// Step 4: Apply IFFT to power spectrum
	//
	// General IFFT formula: IFFT(X) = conj(FFT(conj(X))) / N
	//
	// In our case:
	// - Input X (power spectrum) is real: conj(X) = X
	// - Input X is even symmetric (after Step 3.1)
	// - FFT of real+even symmetric signal yields real+even symmetric output
	// - Therefore: conj(FFT(X)) = FFT(X), so the outer conjugate is identity
	// - Thus: IFFT(X) = FFT(X) / N (both conjugate operations are implicit/unnecessary)
	auto& autocorr = paddedSignal; // Reuse buffer for autocorrelation output
	FFTReal<SIGNAL_LEN_L2 + 1>::execute(spectrum, autocorr, fftWork);
	// The result is a real signal, so the 2nd half (imaginary part) is zero (apart from rounding errors)

	// Step 5: Normalize by 1/FFT_LEN to get autocorrelation (optional)
	// Since FFT(real+even) = real, no conjugate is needed.
	// We only normalize the first N elements that we actually use.
	if constexpr (NORMALIZE) {
		const float norm = 1.0f / FFT_LEN;
		for (unsigned i = 0; i < N; ++i) {
			autocorr[i] *= norm;
		}
	}

	// Step 6: Extract result
	std::ranges::copy_n(autocorr.begin(), N, output.begin());
}

#endif
