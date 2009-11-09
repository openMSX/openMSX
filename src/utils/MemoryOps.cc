// $Id$

#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "likely.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include "systemfuncs.hh"
#include "GLUtil.hh"
#include "Math.hh"
#include "static_assert.hh"
#include "type_traits.hh"
#include "unreachable.hh"
#include <map>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <new> // for std::bad_alloc
#if ASM_X86 && defined _MSC_VER
#include <intrin.h>	// for __stosd intrinsic
#endif

namespace openmsx {

namespace MemoryOps {

// This provides no noticeable performance improvement in
// emulator benchmarks with VC++. Consequently, there's no reason
// to write a Win64 ASM version of this.
#if ASM_X86 && !defined _WIN64
// note: xmm0 must already be filled in
//       bit0 of num is ignored
static inline void memset_128_SSE_streaming(
	unsigned long long* dest, unsigned num)
{
	assert((size_t(dest) & 15) == 0); // must be 16-byte aligned
	unsigned long long* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
#if defined _MSC_VER
		__asm {
			mov				eax,dest
			movntps			xmmword ptr [eax],xmm0
			movntps			xmmword ptr [eax+10h],xmm0
		}
#else
		asm volatile (
			"movntps %%xmm0,   (%0);"
			"movntps %%xmm0, 16(%0);"
			: // no output
			: "r" (dest)
		);
#endif
	}
	if (unlikely(num & 2)) {
#if defined _MSC_VER
		__asm {
			movntps			qword ptr [dest],xmm0
		}
#else
		asm volatile (
			"movntps %%xmm0, (%0);"
			: // no output
			: "r" (dest)
		);
#endif
	}
}

static inline void memset_128_SSE(
	unsigned long long* dest, unsigned num)
{
	assert((size_t(dest) & 15) == 0); // must be 16-byte aligned
	unsigned long long* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
#if defined _MSC_VER
		__asm {
			mov			eax,dest
			movaps		xmmword ptr [eax],xmm0
			movaps		xmmword ptr [eax+10h],xmm0
		}
#else
		asm volatile (
			"movaps %%xmm0,   (%0);"
			"movaps %%xmm0, 16(%0);"
			: // no output
			: "r" (dest)
		);
#endif
	}
	if (unlikely(num & 2)) {
#if defined _MSC_VER
		__asm {
			mov			eax,dest
			movaps		xmmword ptr [eax],xmm0
		}
#else
		asm volatile (
			"movaps %%xmm0, (%0);"
			: // no output
			: "r" (dest)
		);
#endif
	}
}

template<bool STREAMING>
static inline void memset_64_SSE(
	unsigned long long* dest, unsigned num, unsigned long long val)
{
	assert((size_t(dest) & 7) == 0); // must be 8-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(size_t(dest) & 8)) {
		// SSE *must* have 16-byte aligned data
		dest[0] = val;
		++dest; --num;
	}
#if ASM_X86_64
	asm volatile (
		// The 'movd' instruction below actually moves a Quad-word (not a
		// Double word). Though very old binutils don't support the more
		// logical 'movq' syntax. See this bug report for more details.
		//    [2492575] (0.7.0) compilation fails on FreeBSD/amd64
		"movd         %0, %%xmm0;"
		"unpcklps %%xmm0, %%xmm0;"
		: // no output
		: "r" (val)
		#if defined __SSE__
		: "xmm0"
		#endif
	);
#else
	unsigned _low_  = unsigned(val >>  0);
	unsigned _high_ = unsigned(val >> 32);
#if defined _MSC_VER
	__asm {
		movss		xmm0,dword ptr [_low_]
		movss		xmm1,dword ptr [_high_]
		unpcklps	xmm0,xmm0
		unpcklps	xmm1,xmm1
		unpcklps	xmm0,xmm1
	}
#else
	asm volatile (
		"movss        %0, %%xmm0;"
		"movss        %1, %%xmm1;"
		"unpcklps %%xmm0, %%xmm0;"
		"unpcklps %%xmm1, %%xmm1;"
		"unpcklps %%xmm1, %%xmm0;"
		: // no output
		: "m" (_low_)
		, "m" (_high_)
		#if defined __SSE__
		: "xmm0", "xmm1"
		#endif
	);
#endif
#endif
	if (STREAMING) {
		memset_128_SSE_streaming(dest, num);
	} else {
		memset_128_SSE(dest, num);
	}
	if (unlikely(num & 1)) {
		dest[num - 1] = val;
	}
}

static inline void memset_64_MMX(
	unsigned long long* dest, unsigned num, unsigned long long val)
{
    assert((size_t(dest) & 7) == 0); // must be 8-byte aligned

#if defined _MSC_VER
	unsigned lo = unsigned(val >> 0);
	unsigned hi = unsigned(val >> 32);
	unsigned long long* e = dest + num - 3;

	__asm {
		movd		mm0,dword ptr [lo]
		movd		mm1,dword ptr [hi]
		punpckldq	mm0,mm1

		mov			eax,e
		mov			ecx,dest
		mov			edx,dword ptr [num]
mainloop:
		movq		mmword ptr [ecx],mm0
		movq		mmword ptr [ecx+8],mm0
		movq		mmword ptr [ecx+10h],mm0
		movq		mmword ptr [ecx+18h],mm0
		add			ecx,20h
		cmp			ecx,eax
		jb			mainloop

		test		edx,2
		je			test1
		movq		mmword ptr [ecx],mm0
		movq		mmword ptr [ecx+8],mm0
		add			ecx,10h
test1:
		test		edx,1
		je			end
		movq		mmword ptr [ecx],mm0
end:
		emms
	}
#else
	// note can be better on X86_64, but there we anyway use SSE
	asm volatile (
		"movd      %0,%%mm0;"
		"movd      %1,%%mm1;"
		"punpckldq %%mm1,%%mm0;"
		: // no output
		: "r" (unsigned(val >>  0))
		, "r" (unsigned(val >> 32))
		#if defined __MMX__
		: "mm0", "mm1"
		#endif
	);
	unsigned long long* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
		asm volatile (
			"movq %%mm0,   (%0);"
			"movq %%mm0,  8(%0);"
			"movq %%mm0, 16(%0);"
			"movq %%mm0, 24(%0);"
			: // no output
			: "r" (dest)
		);
	}
	if (unlikely(num & 2)) {
		asm volatile (
			"movq %%mm0,  (%0);"
			"movq %%mm0, 8(%0);"
			: // no output
			: "r" (dest)
		);
		dest += 2;
	}
	if (unlikely(num & 1)) {
		asm volatile (
			"movq %%mm0, (%0);"
			: // no output
			: "r" (dest)
		);
	}
	asm volatile ("emms");
#endif
}
#endif

template<bool STREAMING>
static inline void memset_64(
        unsigned long long* dest, unsigned num, unsigned long long val)
{
	assert((size_t(dest) & 7) == 0); // must be 8-byte aligned

#if ASM_X86 && !defined _WIN64
	HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasSSE()) {
		memset_64_SSE<STREAMING>(dest, num, val);
		return;
	}
	if (cpu.hasMMX()) {
		memset_64_MMX(dest, num, val);
		return;
	}
#endif
	unsigned long long* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
		dest[0] = val;
		dest[1] = val;
		dest[2] = val;
		dest[3] = val;
	}
	if (unlikely(num & 2)) {
		dest[0] = val;
		dest[1] = val;
		dest += 2;
	}
	if (unlikely(num & 1)) {
		dest[0] = val;
	}
}

template<bool STREAMING>
static inline void memset_32_2(
	unsigned* dest, unsigned num, unsigned val0, unsigned val1)
{
	assert((size_t(dest) & 3) == 0); // must be 4-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(size_t(dest) & 4)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num;
	}

	unsigned long long val = OPENMSX_BIGENDIAN
		? (static_cast<unsigned long long>(val0) << 32) | val1
		: val0 | (static_cast<unsigned long long>(val1) << 32);
	memset_64<STREAMING>(
		reinterpret_cast<unsigned long long*>(dest), num / 2, val);

	if (unlikely(num & 1)) {
		dest[num - 1] = val0;
	}
}

template<bool STREAMING>
static inline void memset_32(unsigned* dest, unsigned num, unsigned val)
{
	assert((size_t(dest) & 3) == 0); // must be 4-byte aligned

#if ASM_X86
#if defined _MSC_VER
	// VC++'s __stosd intrinsic results in emulator benchmarks
	// running about 7% faster than with memset_32_2, streaming or not,
	// and about 3% faster than the C code below.
	__stosd(reinterpret_cast<unsigned long*>(dest), val, num);
#else
	memset_32_2<STREAMING>(dest, num, val, val);
#endif
#elif defined __arm__
	asm volatile (
		"mov     r3, %[val]\n\t"
		"mov     r4, %[val]\n\t"
		"mov     r5, %[val]\n\t"
		"mov     r6, %[val]\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bmi     1f\n"
		"mov     r7, %[val]\n\t"
		"mov     r8, %[val]\n\t"
		"mov     r9, %[val]\n\t"
		"mov     r10,%[val]\n\t"
	"0:\n\t"
		"stmia   %[dest]!,{r3-r10}\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bpl     0b\n\t"
	"1:\n\t"
		"tst     %[num],#4\n\t"
		"stmneia %[dest]!,{r3-r6}\n\t"
		"tst     %[num],#2\n\t"
		"stmneia %[dest]!,{r3-r4}\n\t"
		"tst     %[num],#1\n\t"
		"strne   r3,[%[dest]]\n\t"

		: [dest] "=r"    (dest)
		, [num] "=r"    (num)
		:       "[dest]" (dest)
		,       "[num]" (num)
		, [val] "r"     (val)
		: "r3","r4","r5","r6","r7","r8","r9","r10"
	);
	return;
#else
	unsigned* e = dest + num - 7;
	for (/**/; dest < e; dest += 8) {
		dest[0] = val;
		dest[1] = val;
		dest[2] = val;
		dest[3] = val;
		dest[4] = val;
		dest[5] = val;
		dest[6] = val;
		dest[7] = val;
	}
	if (unlikely(num & 4)) {
		dest[0] = val;
		dest[1] = val;
		dest[2] = val;
		dest[3] = val;
		dest += 4;
	}
	if (unlikely(num & 2)) {
		dest[0] = val;
		dest[1] = val;
		dest += 2;
	}
	if (unlikely(num & 1)) {
		dest[0] = val;
	}
#endif
}

template<bool STREAMING>
static inline void memset_16_2(
	word* dest, unsigned num, word val0, word val1)
{
	assert((size_t(dest) & 1) == 0); // must be 2-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(size_t(dest) & 2)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num;
	}

	unsigned val = OPENMSX_BIGENDIAN
	             ? (val0 << 16) | val1
	             : val0 | (val1 << 16);
	memset_32<STREAMING>(reinterpret_cast<unsigned*>(dest), num / 2, val);

	if (unlikely(num & 1)) {
		dest[num - 1] = val0;
	}
}

template<bool STREAMING>
static inline void memset_16(word* dest, unsigned num, word val)
{
	memset_16_2<STREAMING>(dest, num, val, val);
}

template <typename Pixel, bool STREAMING>
void MemSet<Pixel, STREAMING>::operator()(
	Pixel* dest, unsigned num, Pixel val) const
{
	if (sizeof(Pixel) == 2) {
		memset_16<STREAMING>(
			reinterpret_cast<word*    >(dest), num, val);
	} else if (sizeof(Pixel) == 4) {
		memset_32<STREAMING>(
			reinterpret_cast<unsigned*>(dest), num, val);
	} else {
		UNREACHABLE;
	}
}

template <typename Pixel, bool STREAMING>
void MemSet2<Pixel, STREAMING>::operator()(
	Pixel* dest, unsigned num, Pixel val0, Pixel val1) const
{
	if (sizeof(Pixel) == 2) {
		memset_16_2<STREAMING>(
			reinterpret_cast<word*    >(dest), num, val0, val1);
	} else if (sizeof(Pixel) == 4) {
		memset_32_2<STREAMING>(
			reinterpret_cast<unsigned*>(dest), num, val0, val1);
	} else {
		UNREACHABLE;
	}
}

// Force template instantiation
template struct MemSet <word,     true >;
template struct MemSet <word,     false>;
template struct MemSet <unsigned, true >;
template struct MemSet <unsigned, false>;
template struct MemSet2<word,     true >;
template struct MemSet2<word,     false>;
template struct MemSet2<unsigned, true >;
template struct MemSet2<unsigned, false>;

#if COMPONENT_GL
#if defined _MSC_VER
// see comment in V9990BitmapConverter
STATIC_ASSERT((is_same_type<unsigned, GLuint>::value));
#else
template<> class MemSet <GLUtil::NoExpansion, true > {};
template<> class MemSet <GLUtil::NoExpansion, false> {};
template<> class MemSet2<GLUtil::NoExpansion, true > {};
template<> class MemSet2<GLUtil::NoExpansion, false> {};
template class MemSet <GLUtil::ExpandGL, true >;
template class MemSet <GLUtil::ExpandGL, false>;
template class MemSet2<GLUtil::ExpandGL, true >;
template class MemSet2<GLUtil::ExpandGL, false>;
#endif
#endif // COMPONENT_GL



void stream_memcpy(unsigned* dst, const unsigned* src, unsigned num)
{
	// 'dst' must be 4-byte aligned. For best performance 'src' should also
	// be 4-byte aligned, but it's not strictly needed.
	assert((size_t(dst) & 3) == 0);
	// VC++'s memcpy function results in emulator benchmarks
	// running about 5% faster than with stream_memcpy.
	// Consequently, we disable this functionality in VC++.
	#if ASM_X86 && !defined _MSC_VER
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasSSE()) {
		if (unlikely(num == 0)) return;
		// align on 8-byte boundary
		if (unlikely(long(dst) & 4)) {
			*dst++ = *src++;
			--num;
		}
	#if defined _MSC_VER
		__asm {
			mov			ebx,dword ptr [src]
			mov			edi,dword ptr [dst]
			mov			esi,dword ptr [num]
			mov         eax,esi
			and         eax,0FFFFFFF0h
			je          label1
			shl         eax,2
			add         ebx,eax
			add         edi,eax
			neg         eax
	mainloop:
			prefetchnta [ebx+eax+140h]
			movq        mm0,mmword ptr [ebx+eax]
			movq        mm1,mmword ptr [ebx+eax+8]
			movq        mm2,mmword ptr [ebx+eax+10h]
			movq        mm3,mmword ptr [ebx+eax+18h]
			movq        mm4,mmword ptr [ebx+eax+20h]
			movq        mm5,mmword ptr [ebx+eax+28h]
			movq        mm6,mmword ptr [ebx+eax+30h]
			movq        mm7,mmword ptr [ebx+eax+38h]
			movntq      mmword ptr [edi+eax],mm0
			movntq      mmword ptr [edi+eax+8],mm1
			movntq      mmword ptr [edi+eax+10h],mm2
			movntq      mmword ptr [edi+eax+18h],mm3
			movntq      mmword ptr [edi+eax+20h],mm4
			movntq      mmword ptr [edi+eax+28h],mm5
			movntq      mmword ptr [edi+eax+30h],mm6
			movntq      mmword ptr [edi+eax+38h],mm7
			add         eax,40h
			jne         mainloop
			and         esi,0Fh
	label1:
			test        esi,8
			je          label2
			movq        mm0,mmword ptr [ebx]
			movq        mm1,mmword ptr [ebx+8]
			movq        mm2,mmword ptr [ebx+10h]
			movq        mm3,mmword ptr [ebx+18h]
			movntq      mmword ptr [edi],mm0
			movntq      mmword ptr [edi+8],mm1
			movntq      mmword ptr [edi+10h],mm2
			movntq      mmword ptr [edi+18h],mm3
			add         ebx,20h
			add         edi,20h
	label2:
			test        esi,4
			je          label3
			movq        mm0,mmword ptr [ebx]
			movq        mm1,mmword ptr [ebx+8]
			movntq      mmword ptr [edi],mm0
			movntq      mmword ptr [edi+8],mm1
			add         ebx,10h
			add         edi,10h
	label3:
			test        esi,2
			je          label4
			movq        mm0,mmword ptr [ebx]
			movntq      mmword ptr [edi],mm0
			add         ebx,8
			add         edi,8
	label4:
			and         esi,1
			je          label5
			mov         edx,dword ptr [ebx]
			mov         dword ptr [edi],edx
	label5:
			emms
		}
		return;
	}
	#else
		// copy chunks of 64 bytes
		unsigned long n2 = num & ~15;
		if (likely(n2)) {
			src += n2;
			dst += n2;
			asm volatile (
				".p2align 4,,15;"
			"0:"
				"prefetchnta 320(%0,%2);"
				"movq    (%0,%2), %%mm0;"
				"movq   8(%0,%2), %%mm1;"
				"movq  16(%0,%2), %%mm2;"
				"movq  24(%0,%2), %%mm3;"
				"movq  32(%0,%2), %%mm4;"
				"movq  40(%0,%2), %%mm5;"
				"movq  48(%0,%2), %%mm6;"
				"movq  56(%0,%2), %%mm7;"
				"movntq  %%mm0,   (%1,%2);"
				"movntq  %%mm1,  8(%1,%2);"
				"movntq  %%mm2, 16(%1,%2);"
				"movntq  %%mm3, 24(%1,%2);"
				"movntq  %%mm4, 32(%1,%2);"
				"movntq  %%mm5, 40(%1,%2);"
				"movntq  %%mm6, 48(%1,%2);"
				"movntq  %%mm7, 56(%1,%2);"
				"add  $64, %2;"
				"jnz   0b;"
				: // no output
				: "r" (src)
				, "r" (dst)
				, "r" (-4 * n2)
				#if defined __MMX__
				: "mm0", "mm1", "mm2", "mm3"
				, "mm4", "mm5", "mm6", "mm7"
				#endif
			);
			num &= 15;
		}
		if (unlikely(num & 8)) {
			asm volatile (
				"movq    (%0), %%mm0;"
				"movq   8(%0), %%mm1;"
				"movq  16(%0), %%mm2;"
				"movq  24(%0), %%mm3;"
				"movntq  %%mm0,   (%1);"
				"movntq  %%mm1,  8(%1);"
				"movntq  %%mm2, 16(%1);"
				"movntq  %%mm3, 24(%1);"
				: // no output
				: "r" (src)
				, "r" (dst)
				#if defined __MMX__
				: "mm0", "mm1", "mm2", "mm3"
				#endif
			);
			src += 8;
			dst += 8;
		}
		if (unlikely(num & 4)) {
			asm volatile (
				"movq    (%0), %%mm0;"
				"movq   8(%0), %%mm1;"
				"movntq  %%mm0,   (%1);"
				"movntq  %%mm1,  8(%1);"
				: // no output
				: "r" (src)
				, "r" (dst)
				#if defined __MMX__
				: "mm0", "mm1"
				#endif
			);
			src += 4;
			dst += 4;
		}
		if (unlikely(num & 2)) {
			asm volatile (
				"movq    (%0), %%mm0;"
				"movntq  %%mm0,   (%1);"
				: // no output
				: "r" (src)
				, "r" (dst)
				#if defined __MMX__
				: "mm0"
				#endif
			);
			src += 2;
			dst += 2;
		}
		if (unlikely(num & 1)) {
			*dst = *src;
		}
		asm volatile ( "emms" );
		return;
	}
#endif
	#endif
	memcpy(dst, src, num * sizeof(unsigned));
}

void stream_memcpy(word* dst, const word* src, unsigned num)
{
	// 'dst' must be 2-byte aligned. For best performance 'src' should also
	// be 2-byte aligned, but it's not strictly needed.
	assert((long(dst) & 1) == 0);
	// VC++'s memcpy function results in emulator benchmarks
	// running about 5% faster than with stream_memcpy.
	// Consequently, we disable this functionality in VC++.
	#if ASM_X86 && !defined _MSC_VER
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasSSE()) {
		if (unlikely(!num)) return;
		// align on 4-byte boundary
		if (unlikely(long(dst) & 2)) {
			*dst++ = *src++;
			--num;
		}
		const unsigned* src2 = reinterpret_cast<const unsigned*>(src);
		unsigned*       dst2 = reinterpret_cast<unsigned*>      (dst);
		stream_memcpy(dst2, src2, num / 2);
		if (unlikely(num & 1)) {
			dst[num - 1] = src[num - 1];
		}
		return;
	}
	#endif
	memcpy(dst, src, num * sizeof(word));
}



/** Aligned memory (de)allocation
 */

// Helper class to keep track of aligned/unaligned pointer pairs
class AllocMap
{
public:
	static AllocMap& instance() {
		static AllocMap oneInstance;
		return oneInstance;
	}

	void insert(void* aligned, void* unaligned) {
		assert(allocMap.find(aligned) == allocMap.end());
		allocMap[aligned] = unaligned;
	}

	void* remove(void* aligned) {
		std::map<void*, void*>::iterator it = allocMap.find(aligned);
		assert(it != allocMap.end());
		void* unaligned = it->second;
		allocMap.erase(it);
		return unaligned;
	}

private:
	AllocMap() {}
	~AllocMap() {
		assert(allocMap.empty());
	}

	std::map<void*, void*> allocMap;
};

void* mallocAligned(unsigned alignment, unsigned size)
{
	assert("must be a power of 2" &&
	       Math::powerOfTwo(alignment) == alignment);
	assert(alignment >= sizeof(void*));
#if HAVE_POSIX_MEMALIGN
	void* aligned;
	if (posix_memalign(&aligned, alignment, size)) {
		throw std::bad_alloc();
	}
	#if defined DEBUG
	AllocMap::instance().insert(aligned, aligned);
	#endif
	return aligned;
#elif defined _MSC_VER
	return _aligned_malloc(size, alignment);
#else
	unsigned long t = alignment - 1;
	void* unaligned = malloc(size + t);
	if (!unaligned) {
		throw std::bad_alloc();
	}
	void* aligned = reinterpret_cast<void*>(
		(reinterpret_cast<unsigned long>(unaligned) + t) & ~t);
	AllocMap::instance().insert(aligned, unaligned);
	return aligned;
#endif
}

void freeAligned(void* aligned)
{
#if HAVE_POSIX_MEMALIGN
	#if defined DEBUG
	AllocMap::instance().remove(aligned);
	#endif
	free(aligned);
#elif defined _MSC_VER
	return _aligned_free(aligned);
#else
	void* unaligned = AllocMap::instance().remove(aligned);
	free(unaligned);
#endif
}

} // namespace MemoryOps

} // namespace openmsx
