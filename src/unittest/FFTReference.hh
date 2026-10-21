#ifndef FFT_REFERENCE_HH
#define FFT_REFERENCE_HH

// Helpers for the fft unit tests: a simple double-precision reference FFT that
// produces output in the packed real-FFT layout.

#include "fft.hh"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace fft_reference {

// Expand the (possibly shortened) input of the given mode to all N samples.
inline std::vector<double> expandInput(fft::InputMode mode, std::span<const float> in, unsigned N)
{
	std::vector<double> x(N, 0.0);
	for (unsigned n = 0; n < N; ++n) {
		switch (mode) {
		case fft::InputMode::Full:
			x[n] = in[n];
			break;
		case fft::InputMode::HalfZeroPadded:
			x[n] = (n < N / 2) ? in[n] : 0.0f;
			break;
		case fft::InputMode::Symmetric:
			x[n] = (n <= N / 2) ? in[n] : in[N - n];
			break;
		case fft::InputMode::Complex:
			assert(false);
			break;
		}
	}
	return x;
}

// Recursive radix-2 complex FFT, e^(-2πi) convention, in place.
inline void complexFFT(std::vector<std::complex<double>>& x)
{
	const auto n = x.size();
	if (n == 1) return;
	std::vector<std::complex<double>> even(n / 2), odd(n / 2);
	for (size_t i = 0; i < n / 2; ++i) {
		even[i] = x[2 * i + 0];
		odd [i] = x[2 * i + 1];
	}
	complexFFT(even);
	complexFFT(odd);
	for (size_t k = 0; k < n / 2; ++k) {
		auto t = std::polar(1.0, -2.0 * std::numbers::pi * double(k) / double(n)) * odd[k];
		x[k] = even[k] + t;
		x[k + n / 2] = even[k] - t;
	}
}

// Straightforward O(N^2) DFT, only used to validate complexFFT().
inline std::vector<std::complex<double>> slowDFT(const std::vector<double>& x)
{
	const auto N = x.size();
	std::vector<std::complex<double>> result(N);
	for (size_t k = 0; k < N; ++k) {
		for (size_t n = 0; n < N; ++n) {
			auto phase = -2.0 * std::numbers::pi * double((k * n) % N) / double(N);
			result[k] += x[n] * std::polar(1.0, phase);
		}
	}
	return result;
}

// Spectrum of the real signal 'x' in the packed real-FFT output layout (N values):
//   out[k]     = Re X[k]   for k = 0 .. N/2
//   out[N/2+k] = Im X[k]   for k = 1 .. N/2-1
inline std::vector<double> toPackedRealLayout(const std::vector<std::complex<double>>& X)
{
	const auto N = X.size();
	std::vector<double> out(N);
	for (size_t k = 0; k <= N / 2; ++k) out[k] = X[k].real();
	for (size_t k = 1; k < N / 2; ++k) out[N / 2 + k] = X[k].imag();
	return out;
}

inline std::vector<double> referenceFFT(const std::vector<double>& x)
{
	std::vector<std::complex<double>> X(x.begin(), x.end());
	complexFFT(X);
	return toPackedRealLayout(X);
}

inline std::vector<double> referenceFFT(fft::InputMode mode, std::span<const float> in, unsigned N)
{
	assert(mode != fft::InputMode::Complex);
	return referenceFFT(expandInput(mode, in, N));
}

// Complex N-point FFT. Input is interleaved (re,im)*N. Output is split:
//   out[0 .. N) = Re X[k], out[N .. 2N) = Im X[k]  (e^{-2πi} convention).
inline std::vector<double> referenceComplexFFT(std::span<const float> interleaved)
{
	assert(interleaved.size() % 2 == 0);
	const auto N = interleaved.size() / 2;
	std::vector<std::complex<double>> X(N);
	for (size_t n = 0; n < N; ++n) {
		X[n] = {double(interleaved[2 * n]), double(interleaved[2 * n + 1])};
	}
	complexFFT(X);
	std::vector<double> out(2 * N);
	for (size_t k = 0; k < N; ++k) {
		out[k] = X[k].real();
		out[N + k] = X[k].imag();
	}
	return out;
}

// |X[k]|^2 for k = 0 .. N/2, computed from a spectrum in packed real-FFT layout.
inline std::vector<double> powerSpectrum(const std::vector<double>& spec)
{
	const auto N = spec.size();
	std::vector<double> p(N / 2 + 1);
	for (size_t k = 0; k <= N / 2; ++k) {
		double im = (k == 0 || k == N / 2) ? 0.0 : spec[N / 2 + k];
		p[k] = spec[k] * spec[k] + im * im;
	}
	return p;
}

// Relative L2 error: |got - ref| / |ref| over the first 'len' values.
inline double relativeError(std::span<const float> got, const std::vector<double>& ref, size_t len)
{
	assert(got.size() >= len && ref.size() >= len);
	double e2 = 0.0, r2 = 0.0;
	for (size_t i = 0; i < len; ++i) {
		double d = double(got[i]) - ref[i];
		e2 += d * d;
		r2 += ref[i] * ref[i];
	}
	return std::sqrt(e2 / r2);
}

// Deterministic pseudo-random values in [-1, 1).
inline std::vector<float> randomSignal(size_t len, uint32_t seed)
{
	std::vector<float> result(len);
	uint32_t s = seed * 2654435761u + 1;
	for (auto& v : result) {
		s = s * 1664525u + 1013904223u; // LCG, high bits are good enough here
		v = float(s >> 8) * (2.0f / float(1 << 24)) - 1.0f;
	}
	return result;
}

} // namespace fft_reference

#endif
