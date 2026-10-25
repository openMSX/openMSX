#ifndef FFT_HH
#define FFT_HH

// Power-of-two FFT for real or complex input (float).
//
// Overview
// --------
// The computational core is always a complex M-point FFT (Stockham auto-sort,
// decimation-in-frequency, forward transform with the e^{-2πi} convention).
// Real-valued input modes pack N = 2M real samples into those M complex
// values, run the same core, then unpack to a real-FFT spectrum. Complex
// input mode is that core directly: M = N = FFT_LEN complex samples.
//
// Real input (Full / HalfZeroPadded / Symmetric)
// ---------------------------------------------
//   z[n] = x[2n] + i·x[2n+1],  n = 0 .. M-1.
//   After the complex FFT of z, unpack recovers X from the pair Z[k], Z[M-k]
//   (the usual real-FFT post-process; equivalent to the classic even/odd form
//   X[k] = E[k] + W^k O[k] with W = e^{-2πi/N}). Packed layout (OUT_MODE =
//   Complex):
//     out[0 .. M]     = Re X[0 .. M]     (DC .. Nyquist)
//     out[M+1 .. N-1] = Im X[1 .. M-1]
//   Symmetric input yields a purely real spectrum; only out[0 .. M] is returned.
//
// Complex input (InputMode::Complex)
// -------------------------------------
//   FFT_LEN complex samples, interleaved as re0,im0,re1,im1,...
//   Work buffers hold 2·FFT_LEN floats. With OUT_MODE = Complex the result is
//   split: out[0 .. N) = Re X[k], out[N .. 2N) = Im X[k] (e^{-2πi} forward FFT).
//
// Output modes
// ------------
//   Complex — spectrum as above (packed real-FFT layout, or split complex).
//   Power   — |X[k]|^2. For real input: k = 0 .. N/2 (N/2+1 values).
//             For complex input: k = 0 .. N-1 (N values). Fused into unpack
//             (real input) or the last FFT pass (complex input).
//
// Auto-vectorization
// ------------------
// Written as portable C++ so GCC/Clang can emit SIMD without intrinsics:
//   * Split real/imag arrays (not arrays of structs) so loads/stores are
//     contiguous floats and map to vector loads.
//   * Stockham (out-of-place) passes: the inner loop runs over a contiguous
//     stride of length >= 8. Twiddles are broadcast; no in-register shuffles
//     in those passes. Indices are signed int so the compiler can prove
//     non-wrapping address arithmetic.
//   * Blocked intermediate layout: each cache line holds 8 reals then 8 imags
//     of the same 8 complex indices. That avoids L1 set conflicts when several
//     streams are spaced by a large power of two. The last pass writes split
//     Re/Im halves for unpack.
//   * First pass (radix-8): vectorizes over the butterfly index p. It must
//     deinterleave the interleaved input and transpose an 8×8 block into the
//     blocked layout — that pass is shuffle-bound; later passes are not.
//   * Small radix loops (load / twiddle / store) are force-unrolled via
//     fft::unroll<N> so the compiler sees straight-line code and constant
//     indices (usable in if constexpr).
//   * Twiddle tables are constexpr and laid out so the hot loops read
//     sequential (or broadcast) floats.
//   * Unpack writes several streams into one buffer; those streams are passed
//     as distinct __restrict pointers (and the loop is not inlined) so the
//     compiler can vectorize without a run-time alias check.
//
// Size: FFT_LEN_L2 in 4 .. 16 → 16 .. 65536 real points, or 4 .. 15 →
// 16 .. 32768 complex points. The complex length M must be at least 8 (leading
// radix-8), and at least 16 for complex input. The first pass vectorizes 8
// butterflies at a time, so M < 64 runs it as a single partial (scalar) block.

#include "cstd.hh"
#include "inline.hh"
#include "stl.hh"

#include <algorithm>
#include <array>
#include <numbers>
#include <span>
#include <type_traits>
#include <utility>

namespace fft {

// Input shape / specialization.
enum class InputMode {
	Full,           // All FFT_LEN real samples
	HalfZeroPadded, // First FFT_LEN/2 samples; second half is zero
	Symmetric,      // First FFT_LEN/2+1 samples; even extension x[N-k]=x[k]
	                // (e.g. N=8: [a,b,c,d,e] → [a,b,c,d,e,d,c,b])
	Complex,        // FFT_LEN complex samples, interleaved re0,im0,re1,im1,...
};

// What execute() writes into the output buffer.
enum class OutputMode {
	Complex, // Full spectrum (packed real-FFT layout, or split Re/Im for
	         // Complex input). Symmetric real input: only the real parts.
	Power,   // |X[k]|^2 (see file comment for the k-range).
};

template<typename F, int... Is>
ALWAYS_INLINE void unrollImpl(F& f, std::integer_sequence<int, Is...>)
{
	(f(std::integral_constant<int, Is>{}), ...);
}

// Calls f(std::integral_constant<int, I>{}) for I = 0 .. N-1, in order.
// The calls are expanded at compile time, so 'I' can be used in constant
// expressions (e.g. 'if constexpr') and the loop is always unrolled.
template<int N, typename F>
ALWAYS_INLINE void unroll(F&& f)
{
	unrollImpl(f, std::make_integer_sequence<int, N>{});
}

// Forward (e^-2πi) DFTs on split arrays, in place, natural order.
ALWAYS_INLINE void dft2(std::span<float, 2> r, std::span<float, 2> i)
{
	float r0 = r[0], i0 = i[0];
	r[0] = r0 + r[1]; i[0] = i0 + i[1];
	r[1] = r0 - r[1]; i[1] = i0 - i[1];
}

ALWAYS_INLINE void dft4(std::span<float, 4> r, std::span<float, 4> i)
{
	float t0r = r[0] + r[2], t0i = i[0] + i[2];
	float t1r = r[0] - r[2], t1i = i[0] - i[2];
	float t2r = r[1] + r[3], t2i = i[1] + i[3];
	float t3r = r[1] - r[3], t3i = i[1] - i[3];
	r[0] = t0r + t2r; i[0] = t0i + t2i;
	r[2] = t0r - t2r; i[2] = t0i - t2i;
	r[1] = t1r + t3i; i[1] = t1i - t3r;
	r[3] = t1r - t3i; i[3] = t1i + t3r;
}

// HALF: inputs 4..7 are zero.
template<bool HALF = false>
ALWAYS_INLINE void dft8(std::span<float, 8> r, std::span<float, 8> i)
{
	constexpr float h = std::numbers::sqrt2_v<float> * 0.5f;
	std::array<float, 4> ur, ui, vr, vi;
	unroll<4>([&](auto k) {
		if constexpr (HALF) {
			ur[k] = r[k]; ui[k] = i[k];
			vr[k] = r[k]; vi[k] = i[k];
		} else {
			ur[k] = r[k] + r[k + 4]; ui[k] = i[k] + i[k + 4];
			vr[k] = r[k] - r[k + 4]; vi[k] = i[k] - i[k + 4];
		}
	});
	// v[k] *= e^(-iπk/4)
	float v1r = (vr[1] + vi[1]) * h, v1i = (vi[1] - vr[1]) * h;
	float v2r = vi[2],               v2i = -vr[2];
	float v3r = (vi[3] - vr[3]) * h, v3i = (vr[3] + vi[3]) * -h;
	vr[1] = v1r; vi[1] = v1i; vr[2] = v2r; vi[2] = v2i; vr[3] = v3r; vi[3] = v3i;
	dft4(ur, ui);
	dft4(vr, vi);
	unroll<4>([&](auto j) {
		r[2 * j + 0] = ur[j]; i[2 * j + 0] = ui[j];
		r[2 * j + 1] = vr[j]; i[2 * j + 1] = vi[j];
	});
}

template<unsigned R>
ALWAYS_INLINE void dft(std::span<float, R> r, std::span<float, R> i)
{
	if constexpr (R == 2) dft2(r, i);
	else if constexpr (R == 4) dft4(r, i);
	else if constexpr (R == 8) dft8(r, i);
	else static_assert(false, "unsupported radix");
}

struct CosSin { float c, s; };

// cos and sin of 2πk/N for the first octant, k = 0 .. N/8, computed in double
// at compile time. For angles <= π/4, 3 iterations already give correctly
// rounded floats.
constexpr double twoPi = 2.0 * std::numbers::pi;
template<unsigned N>
inline constexpr auto octant = []{
	std::array<CosSin, N / 8 + 1> t = {};
	for (unsigned k = 0; k <= N / 8; ++k) {
		t[k] = {float(cstd::cos<3>(twoPi * k / N)),
		        float(cstd::sin<3>(twoPi * k / N))};
	}
	return t;
}();

// cos and sin of 2πk/N. Every angle maps onto the first octant by an exact
// (integer) symmetry, so only N/8 + 1 angles are evaluated per N. That is
// cheaper and more accurate than reducing the angle in floating point.
template<unsigned N>
constexpr CosSin unitRoot(unsigned k)
{
	if constexpr (N < 8) {
		return unitRoot<8>(k * (8 / N));
	} else {
		k %= N;
		if (k > N / 2) { auto [c, s] = unitRoot<N>(N - k);     return {c, -s}; }
		if (k > N / 4) { auto [c, s] = unitRoot<N>(N / 2 - k); return {-c, s}; }
		if (k > N / 8) { auto [c, s] = unitRoot<N>(N / 4 - k); return {s, c}; }
		return octant<N>[k];
	}
}

// Twiddle tables. They depend only on the transform sizes (not on the input
// or output mode), so they live at namespace scope and are shared by all
// FFT<> instantiations that need them.

// First pass of an M-point complex FFT (radix 8, M/8 butterflies p):
// w_M^(p*j) for j = 1..7. The pass is vectorized over p, so the twiddles are
// grouped per block of 8 consecutive p's: t[p / 8][j - 1].re/im[p % 8].
struct TwBlock { std::array<float, 8> re, im; };
template<unsigned M>
alignas(64) inline constexpr auto twFirst = []{
	constexpr unsigned P0 = M / 8;
	std::array<std::array<TwBlock, 7>, (P0 + 7) / 8> t = {};
	for (unsigned b = 0; b < t.size(); ++b) {
		for (unsigned j = 1; j < 8; ++j) {
			auto& [re, im] = t[b][j - 1];
			for (unsigned l = 0; l < std::min(P0, 8u); ++l) {
				const auto [c, s] = unitRoot<M>((8 * b + l) * j);
				re[l] =  c;
				im[l] = -s;
			}
		}
	}
	return t;
}();

// Later pass with radix R at current length LEN: w_LEN^(p*j), j = 1..R-1,
// p-major (broadcast in the inner loop).
template<unsigned R, unsigned LEN, bool IMAG>
alignas(64) inline constexpr auto twStride = []{
	std::array<float, (LEN / R) * (R - 1)> t = {};
	for (unsigned p = 0; p < LEN / R; ++p) {
		for (unsigned j = 1; j < R; ++j) {
			const auto [c, s] = unitRoot<LEN>(p * j);
			t[p * (R - 1) + j - 1] = IMAG ? -s : c;
		}
	}
	return t;
}();

// Real-FFT unpack for N = 2M real points: cos, sin(2πk/N) / 2, k = 1 .. M/2.
template<unsigned M, bool SIN>
alignas(64) inline constexpr auto twUnpack = []{
	std::array<float, M / 2> t = {};
	for (unsigned k = 1; k <= M / 2; ++k) {
		const auto [c, s] = unitRoot<2 * M>(k);
		t[k - 1] = 0.5f * (SIN ? s : c);
	}
	return t;
}();

template<unsigned FFT_LEN_L2, InputMode MODE = InputMode::Full,
         OutputMode OUT_MODE = OutputMode::Complex,
         unsigned... RADICES_L2>
class FFT
{
public:
	static constexpr bool COMPLEX_IN = MODE == InputMode::Complex;
	// Complex FFT length M: 8 (leading radix-8 pass) .. 32768. Complex input
	// needs M >= 16, so that there is a stride pass to fuse Power output into.
	static_assert(FFT_LEN_L2 >= 4);
	static_assert((COMPLEX_IN ? FFT_LEN_L2 : FFT_LEN_L2 - 1) <= 15);
	static constexpr unsigned FFT_LEN = 1 << FFT_LEN_L2;
	// Complex arrays need 2 floats per sample; real-FFT modes reuse FFT_LEN.
	static constexpr unsigned WORK_LEN = COMPLEX_IN ? (2 * FFT_LEN) : FFT_LEN;
	static constexpr unsigned INPUT_LEN =
		COMPLEX_IN ? WORK_LEN :
		(MODE == InputMode::HalfZeroPadded) ? (FFT_LEN / 2) :
		(MODE == InputMode::Symmetric) ? (FFT_LEN / 2 + 1) :
		FFT_LEN;
	static constexpr unsigned OUTPUT_LEN =
		(OUT_MODE == OutputMode::Power) ? (COMPLEX_IN ? FFT_LEN : FFT_LEN / 2 + 1) :
		COMPLEX_IN ? WORK_LEN :
		(MODE == InputMode::Symmetric) ? (FFT_LEN / 2 + 1) :
		FFT_LEN;

	// 'output' and 'tmpBuf' must each hold WORK_LEN floats. The returned span
	// aliases the first OUTPUT_LEN elements of 'output'. Prefer 64-byte
	// aligned buffers (e.g. alignas(64) std::array) for SIMD loads.
	static std::span<float, OUTPUT_LEN> execute(std::span<const float, INPUT_LEN> input,
	                                            std::span<float, WORK_LEN> output,
	                                            std::span<float, WORK_LEN> tmpBuf)
	{
		// Ping-pong between the two buffers. For real input the last pass
		// lands in tmpBuf, followed by an out-of-place unpack; for complex
		// input it writes output directly.
		const std::array<float*, 2> bufs = {output.data(), tmpBuf.data()};
		float* first = bufs[bufIdx(0)];
		if constexpr (MODE == InputMode::Symmetric) {
			// Expand even symmetry into the other buffer, then run the Full
			// first pass (contiguous loads vectorize; on-the-fly remap does not).
			float* full = bufs[1 - bufIdx(0)];
			mirror(input.data(), full);
			firstPass<InputMode::Full>(full, first);
		} else {
			firstPass<MODE>(input.data(), first);
		}
		laterPasses<1, M / 8, 8>(bufs); // after the radix-8 first pass
		if constexpr (!COMPLEX_IN) {
			unpack(tmpBuf.data(), output.data());
		}
		return std::span<float, OUTPUT_LEN>(output.data(), OUTPUT_LEN);
	}

private:
	static constexpr unsigned N = FFT_LEN;
	// Complex FFT length: N for Complex input, N/2 when packing a real FFT.
	static constexpr unsigned M = COMPLEX_IN ? N : N / 2;
	static constexpr unsigned M_L2 = COMPLEX_IN ? FFT_LEN_L2 : FFT_LEN_L2 - 1;

	// Radix plan for the complex FFT: first pass is radix-8, the rest either
	// given explicitly or radix-4 (plus a final radix-8 when that fits, or a
	// trailing radix-2 when only one bit remains). Calls emit(log2(R)) for
	// each pass, in order.
	static constexpr auto planPasses = [](auto emit) {
		emit(3);
		if constexpr (sizeof...(RADICES_L2) > 0) {
			(emit(RADICES_L2), ...);
		} else {
			constexpr unsigned left0 = M_L2 - 3;
			// Prefer a trailing radix-8 when the remainder is odd and large
			// enough; otherwise finish with radix-4s and at most one radix-2.
			if constexpr ((left0 & 1) && left0 >= 3) {
				for (unsigned i = 0; i < (left0 - 3) / 2; ++i) emit(2);
				emit(3);
			} else {
				for (unsigned i = 0; i < left0 / 2; ++i) emit(2);
				if constexpr (left0 & 1) emit(1);
			}
		}
	};
	static constexpr unsigned NUM_PASSES = []{
		unsigned n = 0;
		planPasses([&](unsigned) { ++n; });
		return n;
	}();
	static constexpr auto radicesL2 = []{
		std::array<unsigned, NUM_PASSES> r = {};
		unsigned n = 0;
		planPasses([&](unsigned l2) { r[n++] = l2; });
		return r;
	}();
	static_assert(sum(radicesL2) == M_L2);

	static constexpr bool POWER = OUT_MODE == OutputMode::Power;

	// Pass t (0 = first pass) writes bufs[bufIdx(t)] (0 = output, 1 = tmpBuf).
	static constexpr unsigned LAST_IDX = COMPLEX_IN ? 0 : 1;
	static constexpr unsigned bufIdx(unsigned t) { return (LAST_IDX + NUM_PASSES - 1 - t) & 1; }

	// Intermediate layout: blocks of 8 reals + 8 imags (cache line). Element
	// e = 8*eb + l lives at re = A*eb + l, im = B + A*eb + l. The last stride
	// pass writes split layout (B = M) for unpack or the complex output.
	static constexpr int LA = 16, LB = 8;          // blocked
	static constexpr int LA_SPLIT = 8, LB_SPLIT = int(M); // separate halves

	static constexpr unsigned P0 = M / 8; // first-pass butterflies

	// Even extension: [a,b,c,d,e] → [a,b,c,d,e,d,c,b]. The second half is the
	// reverse of the interior (not of the whole first half).
	static void mirror(const float* __restrict in, float* __restrict full)
	{
		std::copy_n(in, M + 1, full);
		std::reverse_copy(in + 1, in + M, full + M + 1);
	}

	// Load the 8 inputs of first-pass butterfly p.
	template<InputMode IN_MODE>
	ALWAYS_INLINE static void loadFirst(const float* __restrict in, int p,
	                                    std::span<float, 8> ar, std::span<float, 8> ai)
	{
		// Not Symmetric: that is expanded by mirror() and then loaded as Full.
		static_assert(IN_MODE == InputMode::Full || IN_MODE == InputMode::Complex ||
		              IN_MODE == InputMode::HalfZeroPadded);
		// Full: pack pairs of reals as complex. Complex: interleaved input.
		// HalfZeroPadded: inputs 4..7 are zero; dft8<true> doesn't read them.
		unroll<8>([&](auto k) {
			if constexpr (IN_MODE != InputMode::HalfZeroPadded || k < 4) {
				const int n = p + k * int(P0);
				ar[k] = in[2 * n + 0];
				ai[k] = in[2 * n + 1];
			}
		});
	}

	// First pass: radix 8, stride 1, length M. Vectorized over the butterfly
	// index p: the inner loop covers 8 consecutive p's (one twiddle block).
	// M < 64 has a single, partial block.
	template<InputMode IN_MODE>
	static void firstPass(const float* __restrict in, float* __restrict out)
	{
		float* yr = out;
		float* yi = out + LB;
		const auto& tw = twFirst<M>;
		constexpr bool HALF = IN_MODE == InputMode::HalfZeroPadded;
		constexpr int A = LA;
		constexpr int NB = (int(P0) + 7) / 8;
		constexpr int BL = std::min(int(P0), 8);
		for (int b = 0; b < NB; ++b) {
			for (int l = 0; l < BL; ++l) {
				const int p = 8 * b + l;
				std::array<float, 8> ar, ai;
				loadFirst<IN_MODE>(in, p, ar, ai);
				dft8<HALF>(ar, ai);
				yr[A * p] = ar[0];
				yi[A * p] = ai[0];
				unroll<7>([&](auto j1) {
					constexpr int j = j1 + 1;
					const float wr = tw[b][j1].re[l];
					const float wi = tw[b][j1].im[l];
					yr[A * p + j] = ar[j] * wr - ai[j] * wi;
					yi[A * p + j] = ar[j] * wi + ai[j] * wr;
				});
			}
		}
	}

	// Later passes: radix R, current length LEN, stride STRIDE (LEN * STRIDE == M).
	// Source is always blocked; destination is blocked except on the LAST pass,
	// which writes separate real/imag halves (or |Z|^2 for complex input with
	// Power output).
	template<unsigned R, unsigned LEN, unsigned STRIDE, bool LAST>
	static void stridePass(const float* __restrict src, float* __restrict dst)
	{
		static_assert(LEN * STRIDE == M);
		static_assert(STRIDE % 8 == 0);
		constexpr int OA = LAST ? LA_SPLIT : LA;
		constexpr int OB = LAST ? LB_SPLIT : LB;
		constexpr bool OUT_POWER = LAST && COMPLEX_IN && POWER;
		const float* xr = src;
		const float* xi = src + LB;
		float* yr = dst;
		float* yi = dst + OB;
		const float* twr = twStride<R, LEN, false>.data();
		const float* twi = twStride<R, LEN, true>.data();
		constexpr int m = LEN / R;
		constexpr int SB = STRIDE / 8; // stride in blocks of 8
		constexpr int IA = LA;
		for (int p = 0; p < m; ++p) {
			std::array<float, R> wr, wi;
			unroll<R - 1>([&](auto j1) {
				wr[j1 + 1] = twr[p * (int(R) - 1) + j1];
				wi[j1 + 1] = twi[p * (int(R) - 1) + j1];
			});
			for (int qb = 0; qb < SB; ++qb) {
				for (int l = 0; l < 8; ++l) {
					std::array<float, R> ar, ai;
					unroll<R>([&](auto k) {
						const int e = IA * (qb + SB * (p + k * m)) + l;
						ar[k] = xr[e];
						ai[k] = xi[e];
					});
					dft<R>(ar, ai);
					unroll<R>([&](auto j) {
						const int e = OA * (qb + SB * (int(R) * p + j)) + l;
						if constexpr (OUT_POWER) {
							static_assert(m == 1); // last pass: no twiddles
							yr[e] = ar[j] * ar[j] + ai[j] * ai[j];
						} else if constexpr (m == 1 || j == 0) {
							yr[e] = ar[j];
							yi[e] = ai[j];
						} else {
							yr[e] = ar[j] * wr[j] - ai[j] * wi[j];
							yi[e] = ar[j] * wi[j] + ai[j] * wr[j];
						}
					});
				}
			}
		}
	}

	// Run passes PASS .. NUM_PASSES-1 at current length LEN and stride STRIDE.
	template<unsigned PASS, unsigned LEN, unsigned STRIDE>
	static void laterPasses(std::span<float* const, 2> bufs)
	{
		if constexpr (PASS < NUM_PASSES) {
			constexpr unsigned R = 1u << radicesL2[PASS];
			stridePass<R, LEN, STRIDE, (PASS + 1 == NUM_PASSES)>(bufs[bufIdx(PASS - 1)], bufs[bufIdx(PASS)]);
			laterPasses<PASS + 1, LEN / R, STRIDE * R>(bufs);
		}
	}

	// Real-input unpack: X[k] = E[k] + W^k O[k], X[M-k] = conj(E[k] - W^k O[k]).
	static constexpr unsigned UK = M / 2; // k = 1 .. M/2

	// One unpack pair: from Z[k] and Z[M-k] compute bins k and M-k.
	// c, s = cos, sin(2πk/N) / 2; valid for any 0 < k < M.
	// Writes out[k], out[M-k], out[M+k], out[2M-k] via oLo[i], oLoR[-i],
	// oHi[i], oHiR[-i] (see unpackRange).
	ALWAYS_INLINE static void unpackPair(
		float zkr, float zki, float zmr, float zmi, float c, float s,
		float* oLo, float* oLoR, float* oHi, float* oHiR, int i)
	{
		const float er = 0.5f * (zkr + zmr);
		const float sr = zki + zmi;
		const float dr = zmr - zkr;
		const float br = sr * c + dr * s;
		const float xr1 = er + br; // Re X[k]
		const float xr2 = er - br; // Re X[M-k]
		if constexpr (MODE == InputMode::Symmetric) {
			// The spectrum is real.
			oLo[i]   = POWER ? xr1 * xr1 : xr1;
			oLoR[-i] = POWER ? xr2 * xr2 : xr2;
			return;
		}
		const float ei = 0.5f * (zki - zmi);
		const float bi = dr * c - sr * s;
		const float im1 = ei + bi; // Im X[k]
		const float im2 = bi - ei; // Im X[M-k]
		if constexpr (POWER) {
			oLo[i]   = xr1 * xr1 + im1 * im1;
			oLoR[-i] = xr2 * xr2 + im2 * im2;
		} else {
			oLo[i]   = xr1;
			oLoR[-i] = xr2;
			oHi[i]   = im1;
			oHiR[-i] = im2;
		}
	}

	// Unpack k = BEGIN .. END-1. The four output streams are separate
	// restrict pointers into 'out', each pointing to the element for
	// k = BEGIN: out + BEGIN, out + M - BEGIN, out + M + BEGIN, out + 2M - BEGIN.
	// Their accessed ranges don't overlap, but with a single 'out' pointer
	// clang can't prove that. Not inlined: clang loses the restrict guarantee
	// for the loop after inlining.
	template<int BEGIN, int END>
	NEVER_INLINE static void unpackRange(
		const float* __restrict zr, const float* __restrict zi,
		float* __restrict oLo, float* __restrict oLoR,
		float* __restrict oHi, float* __restrict oHiR,
		const float* __restrict twc, const float* __restrict tws)
	{
		for (int k = BEGIN; k < END; ++k) {
			unpackPair(zr[k], zi[k], zr[M - k], zi[M - k], twc[k - 1], tws[k - 1],
			           oLo, oLoR, oHi, oHiR, k - BEGIN);
		}
	}
	template<int BEGIN, int END>
	ALWAYS_INLINE static void unpackRange(
		const float* zr, const float* zi, float* out, const float* twc, const float* tws)
	{
		unpackRange<BEGIN, END>(zr, zi, out + BEGIN, out + M - BEGIN, out + M + BEGIN, out + 2 * M - BEGIN,
		                        twc, tws);
	}

	static void unpack(const float* __restrict z, float* __restrict out)
	{
		const float* zr = z;
		const float* zi = z + M;
		// k = 0 and M: X[0] = zr[0] + zi[0], X[M] = zr[0] - zi[0] (both real)
		// k = M/2:     X[M/2] = zr[M/2] - i·zi[M/2]  (Im = -zi[M/2])
		const float x0 = zr[0] + zi[0];
		const float xm = zr[0] - zi[0];
		if constexpr (POWER) {
			out[0] = x0 * x0;
			out[M] = xm * xm;
			out[M / 2] = (MODE == InputMode::Symmetric)
				? zr[M / 2] * zr[M / 2]
				: zr[M / 2] * zr[M / 2] + zi[M / 2] * zi[M / 2];
		} else {
			out[0] = x0;
			out[M] = xm;
			out[M / 2] = zr[M / 2];
			if constexpr (MODE != InputMode::Symmetric) {
				out[M + M / 2] = -zi[M / 2];
			}
		}
		const float* twc = twUnpack<M, false>.data();
		const float* tws = twUnpack<M, true>.data();
		if constexpr (UK >= 16) {
			// Main loop k = 1 .. UK-8 (a multiple of 8), then the last 7 bins
			// as one 8-wide block that redoes k = UK-8 (same values).
			unpackRange<1, int(UK) - 7>(zr, zi, out, twc, tws);
			unpackRange<int(UK) - 8, int(UK)>(zr, zi, out, twc, tws);
		} else {
			unpackRange<1, int(UK)>(zr, zi, out, twc, tws);
		}
	}
};

} // namespace fft

#endif
