#ifndef DIVISIONBYCONST_HH
#define DIVISIONBYCONST_HH

#include "build-info.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

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
	void setDivisor(uint32_t divisor);
	inline uint32_t getDivisor() const { return divisor; }

	uint32_t div(uint64_t dividend) const
	{
	#if defined __x86_64 && !defined _MSC_VER
		uint64_t t = (__uint128_t(dividend) * m + a) >> 64;
		return t >> s;
	#elif ASM_X86_32
		uint32_t _ch_ = a >> 32;
		uint32_t _cl_ = uint32_t(a);
		const uint32_t _ah_ = dividend >> 32;
		const uint32_t _al_ = uint32_t(dividend);
		const uint32_t _bh_ = m >> 32;
		const uint32_t _bl_ = uint32_t(m);
	#ifdef _MSC_VER
		uint32_t _s_ = s, result;
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
		uint32_t checkResult = divinC(dividend);
		assert(checkResult == result);
	#endif
		return result;
	#else
		// Split in 3 asm sections to be able to satisfy operand
		// constraints: between two sections gcc can reassign operands
		// to different registers or memory locations. Apparently there
		// are only 3 free registers available on OSX (devel flavour).
		uint32_t _th_, _tl_;
		uint32_t dummy;
		asm (
			"mull	%[BL]\n\t"       // eax = [AH]
			"movl	%%eax,%[TL]\n\t"
			"movl	%%edx,%[TH]\n\t"
			"movl	%[AL],%%eax\n\t"
			"mull	%[BL]\n\t"
			"addl	%%eax,%[CL]\n\t"
			"adcl	%%edx,%[CH]\n\t"
			"adcl	$0,%[TH]\n\t"
			"addl	%[CH],%[TL]\n\t"
			"adcl	$0,%[TH]\n\t"

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
			"movl	%%eax,%[CL]\n\t"
			"movl	%%edx,%[CH]\n\t"
			"movl	%[AL],%%eax\n\t"
			"mull	%[BH]\n\t"
			"addl	%%eax,%[TL]\n\t"
			"adcl	%%edx,%[TH]\n\t"
			"adcl	$0,%[CH]\n\t"
			"addl	%[TH],%[CL]\n\t"
			"adcl	$0,%[CH]\n\t"

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
		uint32_t res;
		uint32_t dummy;
		asm volatile (
			"ldmia	%[RES],{r3,r4,r5,r6,r8}\n\t"

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

			"rsb	%[T],r8,#32\n\t"
			"lsr	%[RES],r6,r8\n\t"
			//"orr	%[RES],%[RES],r3,LSL %[T]\n\t" // not thumb2
			"lsls	r3,r3,%[T]\n\t"
			"orrs	%[RES],%[RES],r3\n\t"
			//"mov	%[AL],r3,LSR r8\n\t"     // high word of result, should be 0

			: [RES] "=r"    (res)
			, [T]   "=&r"   (dummy)
			:       "[RES]" (this)
			, [AL]  "r"     (uint32_t(dividend))
			, [AH]  "r"     (dividend >> 32)
			: "r3","r4","r5","r6","r8"
		);
		return res;
	#else
		return divinC(dividend);
	#endif
	}

	inline uint32_t divinC(uint64_t dividend) const
	{
		uint64_t t1 = uint64_t(uint32_t(dividend)) * uint32_t(m);
		uint64_t t2 = (dividend >> 32) * uint32_t(m);
		uint64_t t3 = uint32_t(dividend) * (m >> 32);
		uint64_t t4 = (dividend >> 32) * (m >> 32);

		uint64_t s1 = uint64_t(uint32_t(a)) + uint32_t(t1);
		uint64_t s2 = (s1 >> 32) + (a >> 32) + (t1 >> 32) + t2;
		uint64_t s3 = uint64_t(uint32_t(s2)) + uint32_t(t3);
		uint64_t s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;

		uint64_t result = s4 >> s;
	#ifdef DEBUG
		// we don't even want this overhead in devel builds
		assert(result == uint32_t(result));
	#endif
		return uint32_t(result);
	}

	uint32_t mod(uint64_t dividend) const
	{
	#ifdef __arm__
		uint32_t res;
		uint32_t dummy;
		asm volatile (
			"ldmia	%[RES],{r3,r4,r5,r6,r8,r9}\n\t"

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

			"rsb	%[T],r8,#32\n\t"
			"lsr	%[RES],r6,r8\n\t"
			//"orr	%[RES],%[RES],r3,LSL %[T]\n\t" // not thumb2
			"lsls	r3,r3,%[T]\n\t"
			"orrs	%[RES],%[RES],r3\n\t"          // RES = quotient (must fit in 32-bit)

			"mul	%[AH],%[RES],r9\n\t"           // AH = q * divisor
			"sub	%[RES],%[AL],%[AH]\n\t"        // RES = D - q*d

			: [RES] "=r"    (res)
			, [T]   "=&r"   (dummy)
			:       "[RES]" (this)
			, [AL]  "r"     (uint32_t(dividend))
			, [AH]  "r"     (dividend >> 32)
			: "r3","r4","r5","r6","r8","r9"
		);
		return res;
	#else
		assert(uint32_t(divisor) == divisor); // must fit in 32-bit
		uint64_t q = div(dividend);
		assert(uint32_t(q) == q); // must fit in 32 bit
		// result fits in 32-bit, so no 64-bit calculations required
		return uint32_t(dividend) - uint32_t(q) * uint32_t(divisor);
	#endif
	}

private:
	// note: order is important for ARM asm routines
	uint64_t m;
	uint64_t a;
	uint32_t s;
	uint32_t divisor; // only used by mod() and getDivisor()
};

} // namespace openmsx

#endif // DIVISIONBYCONST_HH
