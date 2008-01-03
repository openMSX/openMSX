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

	void setDivisor(uint64 divisor);

	unsigned div(uint64 dividend) const
	{
	#ifdef ASM_X86_64
		uint64 t = (__uint128_t(dividend) * m + a) >> 64;
		return t >> s;

	#elif defined (ASM_X86_32)
		unsigned th, tl;
		unsigned ah = dividend >> 32;
		unsigned al = dividend;
		unsigned bh = m >> 32;
		unsigned bl = m;
		unsigned ch = a >> 32;
		unsigned cl = a;
		asm volatile (
			"mov	%[AH],%%eax\n\t"
			"mull	%[BL]\n\t"
			"mov	%%eax,%[TL]\n\t"
			"mov	%%edx,%[TH]\n\t"
			"mov	%[AL],%%eax\n\t"
			"mull	%[BL]\n\t"
			"add	%%eax,%[CL]\n\t"
			"adc	%%edx,%[CH]\n\t"
			"adc	$0,%[TH]\n\t"
			"add	%[CH],%[TL]\n\t"
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

			: [CH] "=rm"  (ch)
			, [CL] "=rm"  (cl)
			, [TH] "=&rm" (th)
			, [TL] "=&rm" (tl)
			:      "[CH]" (ch)
			,      "[CL]" (cl)
			, [AH] "m"    (ah)
			, [AL] "m"    (al)
			, [BH] "m"    (bh)
			, [BL] "m"    (bl)
			: "eax","edx"
		);
		uint64 c = (uint64(ch) << 32) | cl;
		return c >> s;

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
		uint64 t1 = uint64(unsigned(dividend)) * unsigned(m);
		uint64 t2 = (dividend >> 32) * unsigned(m);
		uint64 t3 = unsigned(dividend) * (m >> 32);
		uint64 t4 = (dividend >> 32) * (m >> 32);

		uint64 s1 = uint64(unsigned(a)) + unsigned(t1);
		uint64 s2 = (s1 >> 32) + (a >> 32) + (t1 >> 32) + t2;
		uint64 s3 = uint64(unsigned(s2)) + unsigned(t3);
		uint64 s4 = (s3 >> 32) + (s2 >> 32) + (t3 >> 32) + t4;

		return s4 >> s;
	#endif
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
	uint64 divisor; // only used for mod()
};

#endif // DIVISIONBYCONST_HH
