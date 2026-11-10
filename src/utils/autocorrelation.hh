#ifndef AUTOCORRELATION_HH
#define AUTOCORRELATION_HH

#include "fft.hh"
#include "inline.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <span>

// Compute (the first lags of) the autocorrelation of a real signal:
//     output[τ] = Σ_n signal[n] · signal[n+τ],   τ = 0 .. output.size()-1
//
// Template parameters:
//   FFT_LEN_L2 = log2 of the FFT length F used internally. Requirements:
//     signal.size() + output.size() <= F + 1   (no circular wrap-around for
//                                               the requested lags)
//     output.size() <= F / 2
//     Pick the smallest F that satisfies these: the cost is roughly linear
//     in F. (For all lags of a signal of length N: F >= 2N - 1.)
//   NORMALIZE = false: the result is scaled by F. (Saves nothing: the 1/F
//     is folded into an existing pass.)
//
// Algorithm (Wiener-Khinchin): r = IFFT(|FFT(x)|^2), computed with two
// complex FFTs of length M = F/2 and a fused pass in between:
//   1. De-interleave x into z[n] = x[2n] + i·x[2n+1] (zero padded), stored
//      as split re/im arrays.  Z = FFT_M(z).
//   2. middle(): from the pair Z[k], Z[M-k] compute the power bins of the
//      real FFT of x and immediately pack them for the inverse (see there).
//   3. r[2n] + i·r[2n+1] = FFT_M(D)[n]: a plain forward complex FFT whose
//      last pass writes the lags 0 .. M-1 directly in interleaved (natural)
//      order.
// Compared to "real FFT with Power output, then real FFT of the symmetric
// spectrum", this avoids the two real-FFT unpack passes, the expansion of
// the symmetric spectrum and the upper half of the last pass. The FFT
// inputs are split re/im, so neither first pass has to de-interleave.
//
// Buffers: 2 of F (+ a small gap) floats; the FFT input aliases the work
// buffer that the first pass does not write.
namespace autocorrelation_detail {

// Middle step. N = 2M, W = e^{-2πi/N}, X = real FFT of x (length N).
// From Z[k] and Z[M-k] the usual real-FFT unpack gives
//   E = (Z[k] + conj Z[M-k]) / 2        (spectrum of the even samples)
//   O = (Z[k] - conj Z[M-k]) / 2i       (spectrum of the odd samples)
//   X[k] = E + W^k O,   X[k+M] = E - W^k O
// so the power bins are
//   P[k]   = |E|^2 + |O|^2 + 2 Re(conj(E) W^k O)
//   P[k+M] = |E|^2 + |O|^2 - 2 Re(conj(E) W^k O)
// The autocorrelation is the IDFT of the (real, even) P. Split the lags into
// even and odd, mirroring the forward packing:
//   r[2n] + i·r[2n+1] = (1/N) IDFT_M(C)[n]
//   C[k] = A[k] + i·B[k],  A = P[k] + P[k+M] (real),  B = (P[k] - P[k+M]) W^-k
// A is shared by the pair k, M-k and B[M-k] = conj B[k], so both C[k] and
// C[M-k] come almost for free. Finally an IDFT is turned into the forward
// DFT that fft.hh provides by reversing the index: IDFT_M(C)[n] =
// DFT_M(D)[n] with D[k] = C[(M-k) mod M]. The 1/N scale is folded into D.
template<unsigned M, bool NORMALIZE>
ALWAYS_INLINE void middlePair(
	float zkr, float zki, float zmr, float zmi, float c2, float s2,
	float* oLo, float* oLoR, float* oHi, float* oHiR, int i)
{
	constexpr float SCALE = NORMALIZE ? 1.0f / float(2 * M) : 1.0f;
	// c2, s2 = cos, sin(2πk/N) / 2
	const float sr = zkr + zmr, dr = zkr - zmr;
	const float si = zki + zmi, di = zki - zmi;
	// 2E = (sr, di),  2O = (si, -dr)
	// A = 2(|E|^2 + |O|^2) = (sr^2 + dr^2 + si^2 + di^2) / 2 = |Z[k]|^2 + |Z[M-k]|^2
	// (parallelogram law: shorter, and independent of the sums above)
	const float a  = SCALE * (zkr * zkr + zki * zki + zmr * zmr + zmi * zmi);
	// W^k O = (ur, -ui) / 2
	const float ur = c2 * si - s2 * dr;
	const float ui = c2 * dr + s2 * si;
	// P[k] - P[k+M] = 4 Re(conj(E) W^k O) = 2 (sr·ur - di·ui)
	// B = (P[k] - P[k+M]) W^-k, and (c2, s2) = W^-k / 2, so B = g (c2, s2)
	// with g = 4 (sr·ur - di·ui). The SCALE is folded in.
	const float g  = (4.0f * SCALE) * (sr * ur - di * ui);
	const float br = g * c2, bi = g * s2;
	// C[k] = A + iB = (A - Im B, Re B);  D[k] = C[M-k] = A + i·conj(B)
	oLo[i]   = a + bi; oHi[i]   = br; // D[k]
	oLoR[-i] = a - bi; oHiR[-i] = br; // D[M-k]
}

// k = BEGIN .. END-1. Four distinct __restrict output streams (see
// fft::FFT::unpackRange for why), not inlined for the same reason.
template<unsigned M, bool NORMALIZE, int BEGIN, int END>
NEVER_INLINE void middleRange(
	const float* __restrict zr, const float* __restrict zi,
	float* __restrict oLo, float* __restrict oLoR,
	float* __restrict oHi, float* __restrict oHiR,
	const float* __restrict twc, const float* __restrict tws)
{
	for (int k = BEGIN; k < END; ++k) {
		middlePair<M, NORMALIZE>(zr[k], zi[k], zr[M - k], zi[M - k], twc[k - 1], tws[k - 1],
		                         oLo, oLoR, oHi, oHiR, k - BEGIN);
	}
}
template<unsigned M, bool NORMALIZE, int BEGIN, int END>
ALWAYS_INLINE void middleRange(const float* zr, const float* zi, float* out,
                               const float* twc, const float* tws)
{
	constexpr int Mi = int(M);
	float* oi = out + Mi + int(fft::SPLIT_PAD);
	middleRange<M, NORMALIZE, BEGIN, END>(zr, zi, out + BEGIN, out + Mi - BEGIN,
	                                      oi + BEGIN, oi + Mi - BEGIN, twc, tws);
}

// z: Z split (re[0..M), im[0..M)) -> out: D split (re[0..M), im from M + SPLIT_PAD).
template<unsigned M, bool NORMALIZE>
void middle(const float* __restrict z, float* __restrict out)
{
	constexpr float SCALE = NORMALIZE ? 1.0f / float(2 * M) : 1.0f;
	const float* zr = z;
	const float* zi = z + M;
	// k = 0: E = Re Z[0], O = Im Z[0], both real, W^0 = 1:
	//   P[0] = (E+O)^2, P[M] = (E-O)^2 -> A = 2(E^2+O^2), B = 4EO
	// k = M/2: W^k = -i, |E - iO|^2 == |E + iO|^2 = |Z[M/2]|^2 -> A = 2|Z|^2, B = 0
	const float e0 = zr[0], o0 = zi[0];
	float* outIm = out + M + fft::SPLIT_PAD;
	out[0]   = SCALE * 2.0f * (e0 * e0 + o0 * o0);
	outIm[0] = SCALE * 4.0f * e0 * o0;
	const float hr = zr[M / 2], hi = zi[M / 2];
	out[M / 2]   = SCALE * 2.0f * (hr * hr + hi * hi);
	outIm[M / 2] = 0.0f;

	const float* twc = fft::twUnpack<M, false>.data();
	const float* tws = fft::twUnpack<M, true>.data();
	constexpr int UK = int(M) / 2;
	if constexpr (UK >= 16) {
		// Same split as fft::FFT::unpack: a multiple of 8, then one 8-wide
		// block for the tail (redoing k = UK-8, same values).
		middleRange<M, NORMALIZE, 1, UK - 7>(zr, zi, out, twc, tws);
		middleRange<M, NORMALIZE, UK - 8, UK>(zr, zi, out, twc, tws);
	} else {
		middleRange<M, NORMALIZE, 1, UK>(zr, zi, out, twc, tws);
	}
}

} // namespace autocorrelation_detail

template<unsigned FFT_LEN_L2, bool NORMALIZE = true>
void autocorrelation(std::span<const float> signal, std::span<float> output)
{
	static_assert(FFT_LEN_L2 >= 5);  // complex FFT needs M >= 16
	static_assert(FFT_LEN_L2 <= 16); // M <= 32768
	constexpr unsigned F = 1 << FFT_LEN_L2; // real FFT length
	constexpr unsigned M = F / 2;           // complex FFT length
	const auto N = signal.size();
	const auto L = output.size();
	assert(N + L <= F + 1);
	assert(L <= M);

	using Fwd = fft::FFT<FFT_LEN_L2 - 1, fft::InputMode::SplitComplex, fft::OutputMode::Complex>;
	using FwdHalf = fft::FFT<FFT_LEN_L2 - 1, fft::InputMode::SplitComplexHalf, fft::OutputMode::Complex>;
	using Inv = fft::FFT<FFT_LEN_L2 - 1, fft::InputMode::SplitComplex, fft::OutputMode::InterleavedFirstHalf>;
	static_assert(Fwd::WORK_LEN == F && Inv::WORK_LEN == F);
	static_assert(FwdHalf::inputMayAliasOutput() == Fwd::inputMayAliasOutput());
	// Split-complex FFT inputs have a gap between the re and im halves, so the
	// buffers are a bit larger than the FFT work area (the first F floats).
	constexpr unsigned PAD = fft::SPLIT_PAD;
	alignas(64) std::array<float, F + PAD> bufA, bufB; // ok, uninitialized
	auto work = [](auto& b) { return std::span<float, F>(b.data(), F); };
	auto split = [](auto& b) { return std::span<const float, F + PAD>(b); };
	// The FFT input is only read by the first pass, which writes one of the
	// two work buffers: put the input in the other one.
	auto& sigBuf = Fwd::inputMayAliasOutput() ? bufA : bufB;

	// 1. de-interleave + zero pad: even samples -> [0, M), odd -> [M+PAD, 2M+PAD)
	// z has (N+1)/2 samples: if N <= M its upper half is zero, which the
	// SplitComplexHalf FFT doesn't read (so it doesn't have to be stored).
	const bool half = N <= M;
	{
		float* ev = sigBuf.data();
		float* od = sigBuf.data() + M + PAD;
		const float* in = signal.data();
		const size_t h = N / 2;
		for (size_t i = 0; i < h; ++i) {
			ev[i] = in[2 * i];
			od[i] = in[2 * i + 1];
		}
		size_t ne = h;
		if (N & 1) ev[ne++] = in[N - 1];
		const size_t end = half ? M / 2 : M;
		std::fill(ev + ne, ev + end, 0.0f);
		std::fill(od + h,  od + end, 0.0f);
	}
	// Z (split) in bufA
	if (half) {
		FwdHalf::execute(split(sigBuf), work(bufA), work(bufB));
	} else {
		Fwd::execute(split(sigBuf), work(bufA), work(bufB));
	}

	// 2. middle: bufA (Z, split without gap) -> bufB (D, split with gap)
	autocorrelation_detail::middle<M, NORMALIZE>(bufA.data(), bufB.data());

	// 3. inverse: interleaved lags 0 .. M-1 land in bufA or bufB
	auto& res = Inv::inputMayAliasOutput() ? bufB : bufA;
	auto& tmp = Inv::inputMayAliasOutput() ? bufA : bufB;
	Inv::execute(split(bufB), work(res), work(tmp));
	std::copy_n(res.data(), L, output.data());
}

#endif
