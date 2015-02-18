#ifndef DIVMODBYCONST
#define DIVMODBYCONST

#include "build-info.hh"
#include "type_traits.hh"
#include <type_traits>
#include <cstdint>

/** Utility class to optimize 64-bit divide/module by a 32-bit constant.
 * For 32-bit by 32-bit gcc already does this optimiztion (on 64-bit
 * CPUs gcc also does it for 64-bit operands). This optimization especially
 * helps on CPU without a HW division instruction (like ARM).
 *
 * Usage:
 *   DivModByConst<123> dm;
 *   uint32_t d = dm.div(x);  // equivalent to  d = x / 123;
 *   uint32_t m = dm.mod(x);  // equivalent to  d = x % 123;
 */

namespace DivModByConstPrivate {

template<uint32_t A, uint32_t R = 0> struct log2
	: if_c<A == 0, std::integral_constant<int, R>, log2<A / 2, R + 1>> {};

// Utility class to perform 128-bit by 128-bit division at compilation time
template<uint64_t RH, uint64_t RL, uint64_t QH, uint64_t QL, uint64_t DH, uint64_t DL, uint32_t BITS>
struct Div128_helper
{
	static const uint64_t QL2 = (QL << 1);
	static const uint64_t QH2 = (QH << 1) + (QL2 < QL);
	static const uint64_t RL2 = (RL << 1) + (QH2 < QH);
	static const uint64_t RH2 = (RH << 1) + (RL2 < RL);

	static const bool C = (RH2 != DH) ? (RH2 < DH) : (RL2 < DL);
	static const uint64_t RL3 = C ? RL2 : RL2 - DL;
	static const uint64_t RH3 = C ? RH2 : RH2 - DH - (RL3 > RL2);
	static const uint64_t QL3 = C ? QL2 : QL2 + 1;
	static const uint64_t QH3 = C ? QH2 : ((QL3 != 0) ? QH2 : QH2 + 1);

	using Div = Div128_helper<RH3, RL3, QH3, QL3, DH, DL, BITS - 1>;
	static const uint64_t quotientLow   = Div::quotientLow;
	static const uint64_t quotientHigh  = Div::quotientHigh;
	static const uint64_t remainderLow  = Div::remainderLow;
	static const uint64_t remainderHigh = Div::remainderHigh;
};
template<uint64_t RH, uint64_t RL, uint64_t QH, uint64_t QL, uint64_t DH, uint64_t DL>
struct Div128_helper<RH, RL, QH, QL, DH, DL, 0>
{
	static const uint64_t quotientLow   = QL;
	static const uint64_t quotientHigh  = QH;
	static const uint64_t remainderLow  = RL;
	static const uint64_t remainderHigh = RH;
};
template<uint64_t DividendH, uint64_t DividendL, uint64_t DividerH, uint64_t DividerL>
struct Div128
	: Div128_helper<0, 0, DividendH, DividendL, DividerH, DividerL, 128> {};


// equivalent to the following run-time loop:
//   while (!(M & 1)) {
//       M >>= 1;
//       --S;
//   }
template<uint64_t M, uint32_t S, bool B = M & 1> struct DBCReduce
{
	using R2 = DBCReduce<M / 2, S - 1>;
	static const uint64_t   M2 = R2::M2;
	static const uint32_t S2 = R2::S2;
};
template<uint64_t M, uint32_t S> struct DBCReduce<M, S, true>
{
	static const uint64_t   M2 = M;
	static const uint32_t S2 = S;
};

// equivalent to the following run-tim loop:
//   while (((m_low >> 1) < (m_high >> 1)) && (l > 0)) {
//       m_low >>= 1;
//       m_high >>= 1;
//       --l;
//   }
template<uint64_t AH, uint64_t AL, uint64_t BH, uint64_t BL>
struct DBCReduce2Shift
{
	static const uint64_t AH2 = AH / 2;
	static const uint64_t AL2 = AL / 2 + ((AH2 * 2 != AH) ? (1ull << 63) : 0);
	static const uint64_t BH2 = BH / 2;
	static const uint64_t BL2 = BL / 2 + ((BH2 * 2 != BH) ? (1ull << 63) : 0);
};
template<uint64_t AH, uint64_t AL, uint64_t BH, uint64_t BL, uint32_t L>
struct DBCReduce2Test
{
	using S = DBCReduce2Shift<AH, AL, BH, BL>;
	static const bool C = (S::AH2 != S::BH2) ? (S::AH2 < S::BH2)
	                                         : (S::AL2 < S::BL2);
	static const bool value = C && (L > 0);
};
template<uint64_t AH, uint64_t AL, uint64_t BH, uint64_t BL, uint32_t LL, bool B>
struct DBCReduce2Loop
{
	using S = DBCReduce2Shift<AH, AL, BH, BL>;
	using T = DBCReduce2Test<S::AH2, S::AL2, S::BH2, S::BL2, LL - 1>;
	using R = DBCReduce2Loop<S::AH2, S::AL2, S::BH2, S::BL2, LL - 1, T::value>;
	static const uint64_t MLH = R::MLH;
	static const uint64_t MLL = R::MLL;
	static const uint64_t MHH = R::MHH;
	static const uint64_t MHL = R::MHL;
	static const uint32_t L = R::L;
};
template<uint64_t AH, uint64_t AL, uint64_t BH, uint64_t BL, uint32_t LL>
struct DBCReduce2Loop<AH, AL, BH, BL, LL, false>
{
	static const uint64_t MLH = AH;
	static const uint64_t MLL = AL;
	static const uint64_t MHH = BH;
	static const uint64_t MHL = BL;
	static const uint32_t L = LL;
};
template<uint64_t AH, uint64_t AL, uint64_t BH, uint64_t BL, uint32_t LL>
struct DBCReduce2
{
	using T = DBCReduce2Test<AH, AL, BH, BL, LL>;
	using R = DBCReduce2Loop<AH, AL, BH, BL, LL, T::value>;
	static const uint64_t MLH = R::MLH;
	static const uint64_t MLL = R::MLL;
	static const uint64_t MHH = R::MHH;
	static const uint64_t MHL = R::MHL;
	static const uint32_t L = R::L;
};


template<uint32_t S> struct DBCAlgo1
{
	// division possible by only shifting
	uint32_t operator()(uint64_t dividend) const
	{
		return dividend >> S;
	}
};

static inline uint64_t mla64(uint64_t a, uint64_t b, uint64_t c)
{
	// equivalent to this:
	//    return (__uint128_t(a) * b + c) >> 64;
	uint64_t t1 = uint64_t(uint32_t(a)) * uint32_t(b);
	uint64_t t2 = (a >> 32) * uint32_t(b);
	uint64_t t3 = uint32_t(a) * (b >> 32);
	uint64_t t4 = (a >> 32) * (b >> 32);

	uint64_t s1 = uint64_t(uint32_t(c)) + uint32_t(t1);
	uint64_t s2 = (s1 >> 32) + (c >> 32) + (t1 >> 32) + t2;
	uint64_t s3 = uint64_t(uint32_t(s2)) + uint32_t(t3);
	uint64_t s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;
	return s4;
}

template<uint64_t M, uint32_t S> struct DBCAlgo2
{
	// division possible by multiplication and shift
	uint32_t operator()(uint64_t dividend) const
	{
		using R = DBCReduce<M, S>;
	#if ASM_X86_32 || defined(__arm__)
		const uint32_t _ah_ = R::M2 >> 32;
		const uint32_t _al_ = uint32_t((R::M2 << 32) >> 32); // Suppress VC++ C4310 warning
		const uint32_t _bh_ = dividend >> 32;
		const uint32_t _bl_ = uint32_t(dividend);
	#endif
	#if ASM_X86_32
	#ifdef _MSC_VER
		uint32_t _tl_;
		register uint32_t result;
		__asm {
			// It's worth noting that simple benchmarks show this to be
			// no faster than straight division on an Intel E8400
			//
			// eax and edx are used with mul
			// ecx = bl
			// esi = ah
			mov		ecx,_bl_
			mov		esi,_ah_
			// ebx is th
			mov     eax,esi
			mul     ecx
			mov     _tl_,eax
			mov     ebx,edx
			mov     eax,_al_
			mul     ecx
			add     _tl_,edx
			adc     ebx,0
			// ecx = bh now
			// edi is cl
			mov		ecx,_bh_
			mov     eax,esi
			mul     ecx
			mov     edi,eax
			// esi is ch now
			mov     esi,edx
			mov     eax,_al_
			mul     ecx
			add     _tl_,eax
			adc     ebx,edx
			adc     esi,0
			add     edi,ebx
			adc     esi,0
			// Sadly, no way to make this an immediate in VC++
			mov		cl,byte ptr [R::S2]
			shrd    edi,esi,cl
			mov		result,edi
		}
	#ifdef DEBUG
		uint32_t realResult = uint32_t(mla64(dividend, R::M2, 0) >> R::S2);
		assert(realResult == result);
	#endif
		return result;
	#else
		uint32_t th, tl, ch, cl;
		asm (
			"movl	%[AH],%%eax\n\t"
			"mull	%[BL]\n\t"
			"movl	%%eax,%[TL]\n\t"
			"movl	%%edx,%[TH]\n\t"
			"movl	%[AL],%%eax\n\t"
			"mull	%[BL]\n\t"
			"addl	%%edx,%[TL]\n\t"
			"adcl	$0,%[TH]\n\t"

			"movl	%[AH],%%eax\n\t"
			"mull	%[BH]\n\t"
			"movl	%%eax,%[CL]\n\t"
			"movl	%%edx,%[CH]\n\t"
			"movl	%[AL],%%eax\n\t"
			"mull	%[BH]\n\t"
			"addl	%%eax,%[TL]\n\t"
			"adcl	%%edx,%[TH]\n\t"
			"adcl	$0,%[CH]\n\t"
			"addl	%[TH],%[CL]\n\t"
			"adcl	$0,%[CH]\n\t"

			: [CH] "=&rm" (ch)
			, [TH] "=&r"  (th)
			, [CL] "=rm"  (cl)
			, [TL] "=&rm" (tl)
			: [AH] "g"    (_ah_)
			, [AL] "g"    (_al_)
			, [BH] "rm"   (_bh_)
			, [BL] "[CL]" (_bl_)
			: "eax","edx"
		);
		asm (
			"shrd   %[SH],%[CH],%[CL]\n\t"

			: [CL] "=rm"  (cl)
			: [CH] "r"    (ch)
			,      "[CL]" (cl)
			, [SH] "i"    (R::S2)
		);
		return cl;
	#endif
	#elif defined(__arm__)
		uint32_t res;
		uint32_t th,tl;
		asm volatile (
			"umull	%[TH],%[TL],%[AL],%[BL]\n\t"
			"eors	%[TH],%[TH]\n\t"
			"umlal	%[TL],%[TH],%[AH],%[BL]\n\t"

			"umull	%[BL],%[AL],%[BH],%[AL]\n\t"
			"adds	%[TL],%[TL],%[BL]\n\t"
			"adcs	%[TH],%[TH],%[AL]\n\t"
			"mov	%[TL],#0\n\t"
			"adc	%[TL],%[TL],%[TL]\n\t"
			"umlal	%[TH],%[TL],%[AH],%[BH]\n\t"

			"lsr	%[RES],%[TH],%[S]\n\t"
			//"orr	%[RES],%[RES],%[TL],LSL %[S32]\n\t" // not thumb2
			"lsls	%[TL],%[TL],%[S32]\n\t"
			"orrs	%[RES],%[RES],%[TL]\n\t"
			: [RES] "=r"    (res)
			, [TH]  "=&r"   (th)
			, [TL]  "=&r"   (tl)
			: [AH]  "r"     (_ah_)
			, [AL]  "r"     (_al_)
			, [BH]  "r"     (_bh_)
			, [BL]  "[RES]" (_bl_)
			, [S]   "M"   (R::S2)
			, [S32] "M"   (32 - R::S2)
		);
		return res;
	#else
		uint64_t h = mla64(dividend, R::M2, 0);
		uint64_t result = h >> R::S2;
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	#endif
	}
};

template<uint32_t DIVISOR, uint32_t N> struct DBCAlgo3
{
	// division possible by multiplication, addition and shift
	static const uint32_t S = log2<DIVISOR>::value - 1;
	using D = Div128<1 << S, 0, 0, DIVISOR>;
	static const uint64_t M = D::quotientLow + (D::remainderLow > (DIVISOR / 2));

	uint32_t operator()(uint64_t dividend) const
	{
		using R = DBCReduce<M, S + N>;
	#if ASM_X86_32 || defined(__arm__)
		const uint32_t ah = R::M2 >> 32;
		const uint32_t al = uint32_t(R::M2);
		const uint32_t bh = dividend >> 32;
		const uint32_t bl = dividend;
	#endif
	#if ASM_X86_32
		uint32_t th, tl, ch, cl;
		asm (
			"mov	%[AH],%%eax\n\t"
			"mull	%[BL]\n\t"
			"mov	%%eax,%[TL]\n\t"
			"mov	%%edx,%[TH]\n\t"
			"mov	%[AL],%%eax\n\t"
			"mull	%[BL]\n\t"
			"add	%[AL],%%eax\n\t"
			"adc	%[AH],%%edx\n\t"
			"adc	$0,%[TH]\n\t"
			"add	%%edx,%[TL]\n\t"
			"adc	$0,%[TH]\n\t"

			"mov	%[AH],%%eax\n\t"
			"mull	%[BH]\n\t"
			"mov	%%eax,%[CL]\n\t"
			"mov	%%edx,%[CH]\n\t"
			"mov	%[AL],%%eax\n\t"
			"mull	%[BH]\n\t"
			"add	%%eax,%[TL]\n\t"
			"adc	%%edx,%[TH]\n\t"
			"adc	$0,%[CH]\n\t"
			"add	%[TH],%[CL]\n\t"
			"adc	$0,%[CH]\n\t"

			: [CH] "=&rm" (ch)
			, [TH] "=&r"  (th)
			, [CL] "=rm"  (cl)
			, [TL] "=&rm" (tl)
			: [AH] "g"    (ah)
			, [AL] "g"    (al)
			, [BH] "rm"   (bh)
			, [BL] "[CL]" (bl)
			: "eax","edx"
		);
		asm (
			"shrd   %[SH],%[CH],%[CL]\n\t"

			: [CL] "=rm"  (cl)
			: [CH] "r"    (ch)
			,      "[CL]" (cl)
			, [SH] "i"    (R::S2)
		);
		return cl;

	#elif defined(__arm__)
		uint32_t res;
		uint32_t th,tl;
		asm volatile (
			"umull	%[TH],%[TL],%[AL],%[BL]\n\t"
			"adds	%[TH],%[TH],%[AL]\n\t"
			"adcs	%[TL],%[TL],%[AH]\n\t"
			"mov	%[TH],#0\n\t"
			"adc	%[TH],%[TH],%[TH]\n\t"
			"umlal	%[TL],%[TH],%[AH],%[BL]\n\t"

			"umull	%[BL],%[AL],%[BH],%[AL]\n\t"
			"adds	%[TL],%[TL],%[BL]\n\t"
			"adcs	%[TH],%[TH],%[AL]\n\t"
			"mov	%[TL],#0\n\t"
			"adc	%[TL],%[TL],%[TL]\n\t"
			"umlal	%[TH],%[TL],%[AH],%[BH]\n\t"

			"lsr	%[RES],%[TH],%[S]\n\t"
			//"orr	%[RES],%[RES],%[TL],LSL %[S32]\n\t" // not thumb2
			"lsls	%[TL],%[TL],%[S32]\n\t"
			"orrs	%[RES],%[RES],%[TL]\n\t"
			: [RES] "=r"    (res)
			, [TH]  "=&r"   (th)
			, [TL]  "=&r"   (tl)
			: [AH]  "r"     (ah)
			, [AL]  "r"     (al)
			, [BH]  "r"     (bh)
			, [BL]  "[RES]" (bl)
			, [S]   "M"     (R::S2)
			, [S32] "M"     (32 - R::S2)
		);
		return res;
	#else
		uint64_t h = mla64(dividend, R::M2, R::M2);
		return h >> R::S2;
	#endif
	}
};


template<uint32_t DIVISOR, uint32_t N, typename RM> struct DBCHelper3
	: if_c<RM::MHH == 0, DBCAlgo2<RM::MHL, N + RM::L>
	                   , DBCAlgo3<DIVISOR, N>> {};

template<uint32_t DIVISOR, uint32_t N> struct DBCHelper2
{
	static const uint32_t L = log2<DIVISOR>::value;
	static const uint64_t J = 0xffffffffffffffffull % DIVISOR;
	using K = Div128<1 << L, 0, 0, 0xffffffffffffffffull - J>;

	using M_LOW  = Div128< 1 << L,                    0,              0, DIVISOR>;
	using M_HIGH = Div128<(1 << L) + K::quotientHigh, K::quotientLow, 0, DIVISOR>;
	using R = DBCReduce2<M_LOW ::quotientHigh, M_LOW ::quotientLow,
	                     M_HIGH::quotientHigh, M_HIGH::quotientLow, L>;

	uint32_t operator()(uint64_t dividend) const
	{
		DBCHelper3<DIVISOR, N, R> dbc;
		return dbc(dividend);
	}
};

template<uint32_t DIVISOR, uint32_t SHIFT> struct DBCHelper1
	: if_c<DIVISOR == 1, DBCAlgo1<SHIFT>,
	                     if_c<DIVISOR & 1, DBCHelper2<DIVISOR, SHIFT>
	                                     , DBCHelper1<DIVISOR / 2, SHIFT + 1>>> {};

} // namespace DivModByConstPrivate


template<uint32_t DIVISOR> struct DivModByConst
{
	uint32_t div(uint64_t dividend) const
	{
	#ifdef __x86_64
		// on 64-bit CPUs gcc already does this
		// optimization (and better)
		return uint32_t(dividend / DIVISOR);
	#else
		DivModByConstPrivate::DBCHelper1<DIVISOR, 0> dbc;
		return dbc(dividend);
	#endif
	}

	uint32_t mod(uint64_t dividend) const
	{
		uint64_t result;
	#ifdef __x86_64
		result = dividend % DIVISOR;
	#else
		result = dividend - DIVISOR * div(dividend);
	#endif
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	}
};

#endif // DIVMODBYCONST
