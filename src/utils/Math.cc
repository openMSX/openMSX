// $Id$

#include "Math.hh"

#if defined _MSC_VER && defined __x86_64
#include <emmintrin.h>
#endif

namespace openmsx {

#ifdef _MSC_VER
#ifdef __x86_64

// What follows below are C++ implementations of several C99 math functions missing 
// from VC++'s CRT as of 2008. These implementations using SSE/SSE2 intrinsics
// for floating point operations. The (generally safe) assumption is that those 
// instructions are always available on CPUs capable of running 64-bit Windows.
//
// The assembler in Visual Studio (ml64.exe) no longer allows inline assembly,
// so for ease of reading we've opted to use these intrinsic-based functions instead
// of using the hand-written versions in a separate .asm file. The resulting 
// optimized code is pretty close to the hand-written code.

// gcc-generated data section
//;.LC0:
//;	.long	0
//;	.long	1127219200
//;	.section	.rodata.cst16,"aM",@progbits,16
//;	.align 16
//;.LC1:
//;	.long	4294967295
//;	.long	2147483647
//;	.long	0
//;	.long	0
//;	.section	.rodata.cst8
//;	.align 8
//;.LC2:
//;	.long	4294967295
//;	.long	1071644671
//;	.section	.rodata.cst4,"aM",@progbits,4
//;	.align 4
//;.LC3:
//;	.long	1258291200
//;	.section	.rodata.cst16
//;	.align 16
//;.LC4:
//;	.long	2147483647
//;	.long	0
//;	.long	0
//;	.long	0
//;	.ident	"GCC: (GNU) 4.4.0 20090130 (experimental)"
//;	.section	.note.GNU-stack,"",@progbits
// ML64 hand-written data section:
//LC0 QWORD  4330000000000000h
//LC1 QWORD  7FFFFFFFFFFFFFFFh
//LC2 QWORD  3FDFFFFFFFFFFFFFh
//LC3 DWORD          4B000000h
//LC4 DWORD          7FFFFFFFh

// lrint(): round double according to current floating-point rounding direction
// gcc-generated:
//;	cvtsd2siq	%xmm0, %rax
//;	ret
// ML64 hand-written:
//LEAF_ENTRY lrinta, Math
//    cvtsd2si	rax, xmm0
//    ret
//LEAF_END lrinta, Math
long lrint(double x)
{
	return _mm_cvtsd_si32(_mm_load_sd(&x));
}

// lrintf(): round float according to current floating-point rounding direction
// gcc-generated:
//;	cvtss2siq	%xmm0, %rax
//;	ret
// ML64 hand-written:
//LEAF_ENTRY lrintf, Math
//    cvtss2si    rax, xmm0
//    ret
//LEAF_END lrintf, Math
long lrintf(float x)
{
	return _mm_cvtss_si32(_mm_load_ss(&x));
}

// truncf(): round x to the nearest integer not larger in absolute value
// gcc-generated:
//;	movss	.LC4(%rip), %xmm1
//;	movaps	%xmm0, %xmm2
//;	andps	%xmm1, %xmm2
//;	comiss	.LC3(%rip), %xmm2
//;	jae	.L6
//;	cvttss2si	%xmm0, %eax
//;	cvtsi2ss	%eax, %xmm0
//;.L6:
//;	rep
//;	ret
// ML64 hand-written:
//LEAF_ENTRY truncf, Math
//	movss	    xmm1, LC4
//	movaps	    xmm2, xmm0
//	andps	    xmm2, xmm1
//	comiss	    xmm2, LC3
//	jae	L6
//	cvttss2si	eax, xmm0
//	cvtsi2ss	xmm0, eax
//L6:
//  ret
//LEAF_END truncf, Math
float truncf(float x)
{
	__int64 tempi = _mm_cvttss_si64(_mm_load_ss(&x));
	__m128 xmmx = _mm_cvtsi64_ss(_mm_setzero_ps(), tempi);

	float ret;
	_mm_store_ss(&ret, xmmx);
	return ret;
}

// round(): round to nearest integer, away from zero
// gcc-generated:
//;	movsd	.LC1(%rip), %xmm1
//;	movapd	%xmm0, %xmm2
//;	andpd	%xmm1, %xmm2
//;	comisd	.LC0(%rip), %xmm2
//;	jae	.L2
//;	addsd	.LC2(%rip), %xmm2
//;	andnpd	%xmm0, %xmm1
//;	cvttsd2siq	%xmm2, %rax
//;	cvtsi2sdq	%rax, %xmm2
//;	movapd	%xmm2, %xmm0
//;	orpd	%xmm1, %xmm0
//;.L2:
//;	rep
//;	ret
// ML64 hand-written:
//LEAF_ENTRY round, Math
//	movsd	    xmm1, LC1
//	movapd      xmm2, xmm0
//	andpd	    xmm2, xmm1
//	comisd	    xmm2, LC0
//	jae	L2
//	addsd	    xmm2, LC2
//	andnpd	    xmm1, xmm0
//	cvttsd2si	rax,  xmm2
//	cvtsi2sd	xmm2, rax
//	movapd	    xmm0, xmm2
//	orpd	    xmm0, xmm1
//L2:
//	ret
//LEAF_END round, Math
double round(double x)
{
	const double half = 0.5;
	__m128d xmmhalf = _mm_load_sd(&half);
	__m128d xmmx = _mm_load_sd(&x);

	// Add + or -0.5 depending on sign...
	if (_mm_comige_sd(xmmx, _mm_setzero_pd())) {
		xmmx = _mm_add_sd(xmmx, xmmhalf);
	} else {
		xmmx = _mm_sub_sd(xmmx, xmmhalf);
	}

	// ... then round towards zero
	__int64 tempi64 = _mm_cvttsd_si64(xmmx);
	xmmx = _mm_cvtsi64_sd(_mm_setzero_pd(), tempi64);

	double ret;
	_mm_store_sd(&ret, xmmx);
	return ret;
}

#else

	// Poor man's implementations to get 32-bit openmsx building with VC++

	// lrint(): round double according to current floating-point rounding direction
	long lrint(double x)
	{
		if (x >= 0) {
			long t = long(ceil(x));
			if (t % 2) t--;
			return t;
		} else {
			long t = long(ceil(-x));
			if (t % 2) t--;
			return -t;
		}
	}

	// lrint(): round float according to current floating-point rounding direction
	long lrintf(float x)
	{
		if (x >= 0) {
			long t = long(ceil(x));
			if (t % 2) t--;
			return t;
		} else {
			long t = long(ceil(-x));
			if (t % 2) t--;
			return -t;
		}
	}

	// truncf(): round x to the nearest integer not larger in absolute value
	float truncf(float x)
	{
		return (x < 0) ? ceil(x) : floor(x);
	}

	// round(): round to nearest integer, away from zero
	double round(double x)
	{
		if (x >= 0) {
			double t = ceil(x);
			if ((t - x) > 0.5) {
				t--;
			}
			return t;
		} else {
			double t = ceil(-x);
			if ((t + x) > 0.5) {
				t--;
			}
			return -t;
		}
	}

#endif	// __x86_64
#endif	// _MSC_VER

namespace Math {

unsigned powerOfTwo(unsigned a)
{
	// classical implementation:
	//   unsigned res = 1;
	//   while (a > res) res <<= 1;
	//   return res;

	// optimized version
	a += (a == 0); // can be removed if argument is never zero
	return floodRight(a - 1) + 1;
}

void gaussian2(double& r1, double& r2)
{
	static const double S = 2.0 / RAND_MAX;
	double x1, x2, w;
	do {
		x1 = S * rand() - 1.0;
		x2 = S * rand() - 1.0;
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0);
	w = sqrt((-2.0 * log(w)) / w);
	r1 = x1 * w;
	r2 = x2 * w;
}

} // namespace Math

} // namespace openmsx
