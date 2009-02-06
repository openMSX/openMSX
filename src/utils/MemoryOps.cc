// $Id$

#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "likely.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include "probed_defs.hh"
#include "GLUtil.hh"
#include "Math.hh"
#include "static_assert.hh"
#include "type_traits.hh"
#include <map>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <new> // for std::bad_alloc

namespace openmsx {

namespace MemoryOps {

#ifdef ASM_X86
#ifdef _MSC_VER
// TODO - VC++ ASM implementation
#else
// note: xmm0 must already be filled in
//       bit0 of num is ignored
static inline void memset_128_SSE_streaming(
	unsigned long long* out, unsigned num)
{
	assert((long(out) & 15) == 0); // must be 16-byte aligned
	unsigned long long* e = out + num - 3;
	for (/**/; out < e; out += 4) {
		asm volatile (
			"movntps %%xmm0,   (%0);"
			"movntps %%xmm0, 16(%0);"
			: // no output
			: "r" (out)
		);
	}
	if (unlikely(num & 2)) {
		asm volatile (
			"movntps %%xmm0, (%0);"
			: // no output
			: "r" (out)
		);
	}
}

static inline void memset_128_SSE(
	unsigned long long* out, unsigned num)
{
	assert((long(out) & 15) == 0); // must be 16-byte aligned
	unsigned long long* e = out + num - 3;
	for (/**/; out < e; out += 4) {
		asm volatile (
			"movaps %%xmm0,   (%0);"
			"movaps %%xmm0, 16(%0);"
			: // no output
			: "r" (out)
		);
	}
	if (unlikely(num & 2)) {
		asm volatile (
			"movaps %%xmm0, (%0);"
			: // no output
			: "r" (out)
		);
	}
}

template<bool STREAMING>
static inline void memset_64_SSE(
	unsigned long long* out, unsigned num, unsigned long long val)
{
	assert((long(out) & 7) == 0); // must be 8-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(long(out) & 8)) {
		// SSE *must* have 16-byte aligned data
		out[0] = val;
		++out; --num;
	}
#ifdef ASM_X86_64
	asm volatile (
		// The 'movd' instruction below actually moves a Quad-word (not a
		// Double word). Though very old binutils don't support the more
		// logical 'movq' syntax. See this bug report for more details.
		//    [2492575] (0.7.0) compilation fails on FreeBSD/amd64
		"movd         %0, %%xmm0;"
		"unpcklps %%xmm0, %%xmm0;"
		: // no output
		: "r" (val)
		#ifdef __SSE__
		: "xmm0"
		#endif
	);
#else
	unsigned low  = unsigned(val >>  0);
	unsigned high = unsigned(val >> 32);
	asm volatile (
		"movss        %0, %%xmm0;"
		"movss        %1, %%xmm1;"
		"unpcklps %%xmm0, %%xmm0;"
		"unpcklps %%xmm1, %%xmm1;"
		"unpcklps %%xmm1, %%xmm0;"
		: // no output
		: "m" (low)
		, "m" (high)
		#ifdef __SSE__
		: "xmm0", "xmm1"
		#endif
	);
#endif
	if (STREAMING) {
		memset_128_SSE_streaming(out, num);
	} else {
		memset_128_SSE(out, num);
	}
	if (unlikely(num & 1)) {
		out[num - 1] = val;
	}
}

static inline void memset_64_MMX(
	unsigned long long* out, unsigned num, unsigned long long val)
{
        assert((long(out) & 7) == 0); // must be 8-byte aligned

	// note can be better on X86_64, but there we anyway use SSE
	asm volatile (
		"movd      %0,%%mm0;"
		"movd      %1,%%mm1;"
		"punpckldq %%mm1,%%mm0;"
		: // no output
		: "r" (unsigned(val >>  0))
		, "r" (unsigned(val >> 32))
		#ifdef __MMX__
		: "mm0", "mm1"
		#endif
	);
	unsigned long long* e = out + num - 3;
	for (/**/; out < e; out += 4) {
		asm volatile (
			"movq %%mm0,   (%0);"
			"movq %%mm0,  8(%0);"
			"movq %%mm0, 16(%0);"
			"movq %%mm0, 24(%0);"
			: // no output
			: "r" (out)
		);
	}
	if (unlikely(num & 2)) {
		asm volatile (
			"movq %%mm0,  (%0);"
			"movq %%mm0, 8(%0);"
			: // no output
			: "r" (out)
		);
		out += 2;
	}
	if (unlikely(num & 1)) {
		asm volatile (
			"movq %%mm0, (%0);"
			: // no output
			: "r" (out)
		);
	}
	asm volatile ("emms");
}
#endif
#endif

template<bool STREAMING>
static inline void memset_64(
        unsigned long long* out, unsigned num, unsigned long long val)
{
	assert((long(out) & 7) == 0); // must be 8-byte aligned

#ifdef ASM_X86
#ifdef _MSC_VER
// TODO - VC++ ASM implementation
#else
	HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasSSE()) {
		memset_64_SSE<STREAMING>(out, num, val);
		return;
	}
	if (cpu.hasMMX()) {
		memset_64_MMX(out, num, val);
		return;
	}
#endif
#endif
	unsigned long long* e = out + num - 3;
	for (/**/; out < e; out += 4) {
		out[0] = val;
		out[1] = val;
		out[2] = val;
		out[3] = val;
	}
	if (unlikely(num & 2)) {
		out[0] = val;
		out[1] = val;
		out += 2;
	}
	if (unlikely(num & 1)) {
		out[0] = val;
	}
}

template<bool STREAMING>
static inline void memset_32_2(
	unsigned* out, unsigned num, unsigned val0, unsigned val1)
{
	assert((long(out) & 3) == 0); // must be 4-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(long(out) & 4)) {
		out[0] = val1; // start at odd pixel
		++out; --num;
	}

	unsigned long long val = OPENMSX_BIGENDIAN
		? (static_cast<unsigned long long>(val0) << 32) | val1
		: val0 | (static_cast<unsigned long long>(val1) << 32);
	memset_64<STREAMING>(
		reinterpret_cast<unsigned long long*>(out), num / 2, val);

	if (unlikely(num & 1)) {
		out[num - 1] = val0;
	}
}

template<bool STREAMING>
static inline void memset_32(unsigned* out, unsigned num, unsigned val)
{
	assert((long(out) & 3) == 0); // must be 4-byte aligned

#ifdef ASM_X86
#ifdef _MSC_VER
// TODO - VC++ ASM implementation
#else
	memset_32_2<STREAMING>(out, num, val, val);
	return;
#endif
#endif

#ifdef __arm__
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
		"stmia   %[out]!,{r3-r10}\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bpl     0b\n\t"
	"1:\n\t"
		"tst     %[num],#4\n\t"
		"stmneia %[out]!,{r3-r6}\n\t"
		"tst     %[num],#2\n\t"
		"stmneia %[out]!,{r3-r4}\n\t"
		"tst     %[num],#1\n\t"
		"strne   r3,[%[out]]\n\t"

		: [out] "=r"    (out)
		, [num] "=r"    (num)
		:       "[out]" (out)
		,       "[num]" (num)
		, [val] "r"     (val)
		: "r3","r4","r5","r6","r7","r8","r9","r10"
	);
	return;
#endif

	unsigned* e = out + num - 7;
	for (/**/; out < e; out += 8) {
		out[0] = val;
		out[1] = val;
		out[2] = val;
		out[3] = val;
		out[4] = val;
		out[5] = val;
		out[6] = val;
		out[7] = val;
	}
	if (unlikely(num & 4)) {
		out[0] = val;
		out[1] = val;
		out[2] = val;
		out[3] = val;
		out += 4;
	}
	if (unlikely(num & 2)) {
		out[0] = val;
		out[1] = val;
		out += 2;
	}
	if (unlikely(num & 1)) {
		out[0] = val;
	}
}

template<bool STREAMING>
static inline void memset_16_2(
	word* out, unsigned num, word val0, word val1)
{
	assert((long(out) & 1) == 0); // must be 2-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(long(out) & 2)) {
		out[0] = val1; // start at odd pixel
		++out; --num;
	}

	unsigned val = OPENMSX_BIGENDIAN
	             ? (val0 << 16) | val1
	             : val0 | (val1 << 16);
	memset_32<STREAMING>(reinterpret_cast<unsigned*>(out), num / 2, val);

	if (unlikely(num & 1)) {
		out[num - 1] = val0;
	}
}

template<bool STREAMING>
static inline void memset_16(word* out, unsigned num, word val)
{
        memset_16_2<STREAMING>(out, num, val, val);
}

template <typename Pixel, bool STREAMING>
void MemSet<Pixel, STREAMING>::operator()(
	Pixel* out, unsigned num, Pixel val) const
{
	if (sizeof(Pixel) == 2) {
		memset_16<STREAMING>(
			reinterpret_cast<word*    >(out), num, val);
	} else if (sizeof(Pixel) == 4) {
		memset_32<STREAMING>(
			reinterpret_cast<unsigned*>(out), num, val);
	} else {
		assert(false);
	}
}

template <typename Pixel, bool STREAMING>
void MemSet2<Pixel, STREAMING>::operator()(
	Pixel* out, unsigned num, Pixel val0, Pixel val1) const
{
	if (sizeof(Pixel) == 2) {
		memset_16_2<STREAMING>(
			reinterpret_cast<word*    >(out), num, val0, val1);
	} else if (sizeof(Pixel) == 4) {
		memset_32_2<STREAMING>(
			reinterpret_cast<unsigned*>(out), num, val0, val1);
	} else {
		assert(false);
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

#ifdef COMPONENT_GL
#ifdef _MSC_VER
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
	assert((long(dst) & 3) == 0);
	#ifdef ASM_X86
	#ifdef _MSC_VER
	// TODO - VC++ ASM implementation
	#else
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasSSE()) {
		if (unlikely(num == 0)) return;
		// align on 8-byte boundary
		if (unlikely(long(dst) & 4)) {
			*dst++ = *src++;
			--num;
		}
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
				#ifdef __MMX__
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
				#ifdef __MMX__
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
				#ifdef __MMX__
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
				#ifdef __MMX__
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
	#ifdef ASM_X86
	#ifdef _MSC_VER
	// TODO - VC++ ASM implementation
	#else
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
#ifdef HAVE_POSIX_MEMALIGN
	void* aligned;
	if (posix_memalign(&aligned, alignment, size)) {
		throw std::bad_alloc();
	}
	#ifdef DEBUG
	AllocMap::instance().insert(aligned, aligned);
	#endif
	return aligned;
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
#ifdef HAVE_POSIX_MEMALIGN
	#ifdef DEBUG
	AllocMap::instance().remove(aligned);
	#endif
	free(aligned);
#else
	void* unaligned = AllocMap::instance().remove(aligned);
	free(unaligned);
#endif
}

} // namespace MemoryOps

} // namespace openmsx
