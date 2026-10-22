#ifndef AUTOCORRELATION_HH
#define AUTOCORRELATION_HH

#include "fft.hh"
#include "ranges.hh"

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
//   2. Forward Real FFT + power spectrum: signal → |X[k]|² (even, real)
//   3. Inverse Real FFT: power → autocorrelation (via FFT of even-symmetric input)
template<unsigned SIGNAL_LEN_L2, bool NORMALIZE = true>
void autocorrelation(std::span<const float> signal, std::span<float> output)
{
	static_assert(SIGNAL_LEN_L2 >= 3);  // max signal length at least 8 (FFT needs >= 16)
	static_assert(SIGNAL_LEN_L2 <= 15); // max signal length at most 32768
	static constexpr unsigned SIGNAL_LEN = 1 << SIGNAL_LEN_L2; // Maximum signal length
	static constexpr unsigned FFT_LEN = 2 * SIGNAL_LEN; // For linear correlation padding
	static constexpr unsigned FFT_LEN_L2 = SIGNAL_LEN_L2 + 1;

	const auto N = signal.size();
	assert(N <= SIGNAL_LEN);
	assert(N == output.size());

	// Step 1: Zero-pad signal to FFT_LEN. This serves two purposes:
	// 1. Extend from N to 2*N (required for linear autocorrelation, not circular)
	// 2. Round up to power-of-2 (required by FFT)
	// Padding is:
	// * partially explicit (up-to SIGNAL_LEN)
	// * partially implicit (between SIGNAL_LEN and FFT_LEN) because of InputMode::HalfZeroPadded
	alignas(64) std::array<float, FFT_LEN> workBuf;
	auto paddedSignal = subspan<SIGNAL_LEN>(workBuf);
	std::ranges::copy(signal, paddedSignal.begin());
	std::ranges::fill(std::span(paddedSignal).subspan(N), 0.0f);

	// Step 2: Forward Real FFT + power spectrum |X[k]|² for k = 0 .. SIGNAL_LEN
	// (fused OutputMode::Power). Result is real and even-symmetric.
	alignas(64) std::array<float, FFT_LEN> spectrum;
	alignas(64) std::array<float, FFT_LEN> fftWork;
	auto power = fft::FFT<FFT_LEN_L2, fft::InputMode::HalfZeroPadded, fft::OutputMode::Power>
		::execute(paddedSignal, spectrum, fftWork);

	// Step 3: Apply IFFT to power spectrum
	//
	// General IFFT formula: IFFT(X) = conj(FFT(conj(X))) / N
	//
	// In our case:
	// - Input X (power spectrum) is real: conj(X) = X
	// - Input X is even symmetric (InputMode::Symmetric expands the mirror)
	// - FFT of real+even symmetric signal yields real+even symmetric output
	// - Therefore: conj(FFT(X)) = FFT(X), so the outer conjugate is identity
	// - Thus: IFFT(X) = FFT(X) / N (both conjugate operations are implicit/unnecessary)
	auto& autocorr = workBuf; // Reuse buffer for autocorrelation output
	fft::FFT<FFT_LEN_L2, fft::InputMode::Symmetric>::execute(power, autocorr, fftWork);
	// The result is a real signal, so the 2nd half (imaginary part) is zero (apart from rounding errors)

	// Step 4: Extract the first N elements, optionally normalized by 1/FFT_LEN.
	// Since FFT(real+even) = real, no conjugate is needed.
	auto result = subspan(autocorr, 0, N);
	if constexpr (NORMALIZE) {
		const float norm = 1.0f / FFT_LEN;
		std::ranges::transform(result, output.begin(), [&](float x) { return x * norm; });
	} else {
		std::ranges::copy(result, output.begin());
	}
}

#endif
