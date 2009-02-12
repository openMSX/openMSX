// $Id:$

#ifndef DIVISIONBYCONST_HH
#define DIVISIONBYCONST_HH

#include "build-info.hh"
#include <cassert>

/** Helper class to divide multiple times by the same number.
 * Binary division can be performed by:
 *   - multiplication by a magic number
 *   - followed by an addition of a magic number
 *   - follwed by a right shift over some magic number of bits
 * These magic constants only depend on the divisor, but they are quite
 * expensive to calculate.
 * However if you know you will divide many times by the same number this
 * algorithm does make sense and can result in a big speedup, especially on
 * CPUs which lack a hardware division instruction (like ARM).
 *
 * If the divisor is a compile-time constant, it's even faster to use
 * the DivModByConst utility class.
 */
class DivModBySame
{
public:
	typedef unsigned long long uint64;

	void setDivisor(unsigned divisor);
	inline unsigned getDivisor() const { return divisor; }

	unsigned div(uint64 dividend) const
	{
	#if defined __x86_64 && !defined _MSC_VER
		uint64 t = (__uint128_t(dividend) * m + a) >> 64;
		return t >> s;
	#elif defined (ASM_X86_32)
		unsigned _ch_ = a >> 32;
		unsigned _cl_ = unsigned(a);
		// g++-3.4 gives this warning when the vars below are const.
		//    use of memory input without lvalue in asm operand n is deprecated
		// For newer gcc versions it's ok.
		/*const*/ unsigned _ah_ = dividend >> 32;
		/*const*/ unsigned _al_ = unsigned(dividend);
		/*const*/ unsigned _bh_ = m >> 32;
		/*const*/ unsigned _bl_ = unsigned(m);
	#ifdef _MSC_VER
		unsigned _s_ = s, result;
		__asm {
			// It's worth noting that simple benchmarks show this to be
			// no faster than straight division on an Intel E8400
			//
			// eax and edx are clobbered by mul
			// eax = _ah_
			// ebx is _cl_
			// ecx is _tl_ - not initialized
			// edi is _th_ - not initialized
			// esi is _ch_
			mov		eax,_ah_
			mov		esi,_ch_
			mov		ebx,_cl_

			mul		_bl_
			mov		ecx,eax
			mov		edi,edx
			mov		eax,_al_
			mul		_bl_
			add		ebx,eax
			adc		esi,edx
			adc		edi,0
			add		ecx,esi
			adc		edi,0

			// eax = _ah_
			// ecx is now free - use it for _bh_
			mov		ecx,_bh_
			mov		eax,_ah_

			mul		ecx
			mov		ebx,eax
			mov		esi,edx
			mov		eax,_al_
			mul		ecx
			add		ecx,eax
			adc		edi,edx
			adc		esi,0
			add		ebx,edi
			adc		esi,0

			mov		cl,byte ptr [_s_]
			shrd	ebx,esi,cl
			mov		result,ebx
		}
	#ifdef DEBUG
		unsigned checkResult = divinC(dividend);
		assert(checkResult == result);
	#endif
		return result;
	#else
		// Split in 3 asm sections to be able to satisfy operand
		// constraints: between two sections gcc can reassign operands
		// to different registers or memory locations. Apparently there
		// are only 3 free registers available on OSX (devel flavour).
		unsigned _th_, _tl_;
		unsigned dummy;
		asm (
			"mull	%[BL]\n\t"       // eax = [AH]
			"mov	%%eax,%[TL]\n\t"
			"mov	%%edx,%[TH]\n\t"
			"mov	%[AL],%%eax\n\t"
			"mull	%[BL]\n\t"
			"add	%%eax,%[CL]\n\t"
			"adc	%%edx,%[CH]\n\t"
			"adc	$0,%[TH]\n\t"
			"add	%[CH],%[TL]\n\t"
			"adc	$0,%[TH]\n\t"

			: [TH]  "=&rm"  (_th_)
			, [TL]  "=&r"   (_tl_)
			, [CH]  "=rm"   (_ch_)
			, [CL]  "=rm"   (_cl_)
			, [EAX] "=&a"   (dummy)
			:       "[CH]"  (_ch_)
			,       "[CL]"  (_cl_)
			,       "[EAX]" (_ah_)
			, [AL]  "m"     (_al_)
			, [BL]  "m"     (_bl_)
			: "edx"
		);
		asm (
			"mull	%[BH]\n\t"       // eax = [AH]
			"mov	%%eax,%[CL]\n\t"
			"mov	%%edx,%[CH]\n\t"
			"mov	%[AL],%%eax\n\t"
			"mull	%[BH]\n\t"
			"add	%%eax,%[TL]\n\t"
			"adc	%%edx,%[TH]\n\t"
			"adc	$0,%[CH]\n\t"
			"add	%[TH],%[CL]\n\t"
			"adc	$0,%[CH]\n\t"

			: [CH]  "=rm"   (_ch_)
			, [CL]  "=r"    (_cl_)
			, [TH]  "=&rm"  (_th_)
			, [TL]  "=&rm"  (_tl_)
			, [EAX] "=&a"   (dummy)
			:       "[TH]"  (_th_)
			,       "[TL]"  (_tl_)
			,       "[EAX]" (_ah_)
			, [AL]  "m"     (_al_)
			, [BH]  "m"     (_bh_)
			: "edx"
		);
		asm (
			"shrd   %%cl,%[CH],%[CL]\n\t" // SH = ecx

			: [CL] "=rm"  (_cl_)
			: [CH] "r"    (_ch_)
			,      "[CL]" (_cl_)
			, [SH] "c"    (s)
		);
		return _cl_;
	#endif
	#elif defined(__arm__)
		unsigned res;
		unsigned dummy;
		asm volatile (
			"ldmia	%[RES],{r3,r4,r5,r6,r7}\n\t"

			"umull	%[T],%[RES],%[AL],r3\n\t" // RES:T  = AL * BL
			"adds	%[T],%[T],r5\n\t"         // T += CL
			"adcs	%[RES],%[RES],r6\n\t"     // RES += CH
			"mov	r5,#0\n\t"
			"adc	%[T],r5,r5\n\t"           // T = carry
			"umlal	%[RES],%[T],%[AH],r3\n\t" // T:RES = AH:AL * BL + CH:CL = TH:TL

			"umull	r3,r6,%[AL],r4\n\t"       // r6:r3  = AL * BH
			"adds	r3,r3,%[RES]\n\t"         // r3 += TL
			"adcs	r6,r6,%[T]\n\t"           // r6 += TH
			"adc	r3,r5,r5\n\t"             // r3 = carry
			"umlal	r6,r3,%[AH],r4\n\t"       // r3:r6 = AH:AL * BH:BL + CH:CL

			"rsb	%[T],r7,#32\n\t"
			"mov	%[RES],r6,LSR r7\n\t"
			"orr	%[RES],%[RES],r3,LSL %[T]\n\t"
			//"mov	%[AL],r3,LSR r7\n\t"     // high word of result, should be 0

			: [RES] "=r"    (res)
			, [T]   "=&r"   (dummy)
			:       "[RES]" (this)
			, [AL]  "r"     (unsigned(dividend))
			, [AH]  "r"     (dividend >> 32)
			: "r3","r4","r5","r6","r7"
		);
		return res;
	#else
		return divinC(dividend);
	#endif
	}

	inline unsigned divinC(uint64 dividend) const
	{
		uint64 t1 = uint64(unsigned(dividend)) * unsigned(m);
		uint64 t2 = (dividend >> 32) * unsigned(m);
		uint64 t3 = unsigned(dividend) * (m >> 32);
		uint64 t4 = (dividend >> 32) * (m >> 32);

		uint64 s1 = uint64(unsigned(a)) + unsigned(t1);
		uint64 s2 = (s1 >> 32) + (a >> 32) + (t1 >> 32) + t2;
		uint64 s3 = uint64(unsigned(s2)) + unsigned(t3);
		uint64 s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;

		uint64 result = s4 >> s;
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == unsigned(result));
	#endif
		return unsigned(result);
	}

	unsigned mod(uint64 dividend) const
	{
	#ifdef __arm__
		unsigned res;
		unsigned dummy;
		asm volatile (
			"ldmia	%[RES],{r3,r4,r5,r6,r7,r8}\n\t"

			"umull	%[T],%[RES],%[AL],r3\n\t" // RES:T  = AL * BL
			"adds	%[T],%[T],r5\n\t"         // T += CL
			"adcs	%[RES],%[RES],r6\n\t"     // RES += CH
			"mov	r5,#0\n\t"
			"adc	%[T],r5,r5\n\t"           // T = carry
			"umlal	%[RES],%[T],%[AH],r3\n\t" // T:RES = AH:AL * BL + CH:CL = TH:TL

			"umull	r3,r6,%[AL],r4\n\t"       // r6:r3  = AL * BH
			"adds	r3,r3,%[RES]\n\t"         // r3 += TL
			"adcs	r6,r6,%[T]\n\t"           // r6 += TH
			"adc	r3,r5,r5\n\t"             // r3 = carry
			"umlal	r6,r3,%[AH],r4\n\t"       // r3:r6 = AH:AL * BH:BL + CH:CL

			"rsb	%[T],r7,#32\n\t"
			"mov	%[RES],r6,LSR r7\n\t"
			"orr	%[RES],%[RES],r3,LSL %[T]\n\t" // RES = quotient (must fit in 32-bit)

			"mul	%[AH],%[RES],r8\n\t"           // AH = q * divisor
			"sub	%[RES],%[AL],%[AH]\n\t"        // RES = D - q*d

			: [RES] "=r"    (res)
			, [T]   "=&r"   (dummy)
			:       "[RES]" (this)
			, [AL]  "r"     (unsigned(dividend))
			, [AH]  "r"     (dividend >> 32)
			: "r3","r4","r5","r6","r7","r8"
		);
		return res;
	#else
		assert(unsigned(divisor) == divisor); // must fit in 32-bit
		uint64 q = div(dividend);
		assert(unsigned(q) == q); // must fit in 32 bit
		// result fits in 32-bit, so no 64-bit calculations required
		return unsigned(dividend) - unsigned(q) * unsigned(divisor);
	#endif
	}

private:
	// note: order is important for ARM asm routines
	uint64 m;
	uint64 a;
	unsigned s;
	unsigned divisor; // only used by mod() and getDivisor()
};

#endif // DIVISIONBYCONST_HH
