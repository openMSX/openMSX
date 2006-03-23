// $Id$

#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "likely.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include "probed_defs.hh"
#include <map>
#include <cassert>
#include <cstdlib>

namespace openmsx {

namespace MemoryOps {

static inline void memset4_2_CPP(
	unsigned* p, unsigned n, unsigned val0, unsigned val1)
{
	if (unlikely((long)p & 4)) {
		p[0] = val1; // start at odd pixel
		++p; --n;
	}
	unsigned* e = p + n - 15;
	for (/**/; p < e; p += 16) {
		p[ 0] = val0;
		p[ 1] = val1;
		p[ 2] = val0;
		p[ 3] = val1;
		p[ 4] = val0;
		p[ 5] = val1;
		p[ 6] = val0;
		p[ 7] = val1;
		p[ 8] = val0;
		p[ 9] = val1;
		p[10] = val0;
		p[11] = val1;
		p[12] = val0;
		p[13] = val1;
		p[14] = val0;
		p[15] = val1;
	}
	if (unlikely(n & 8)) {
		p[0] = val0;
		p[1] = val1;
		p[2] = val0;
		p[3] = val1;
		p[4] = val0;
		p[5] = val1;
		p[6] = val0;
		p[7] = val1;
		p += 8;
	}
	if (unlikely(n & 4)) {
		p[0] = val0;
		p[1] = val1;
		p[2] = val0;
		p[3] = val1;
		p += 4;
	}
	if (unlikely(n & 2)) {
		p[0] = val0;
		p[1] = val1;
		p += 2;
	}
	if (unlikely(n & 1)) {
		p[0] = val0;
	}
}

static inline void memset4_2_MMX(
	unsigned* p, unsigned n, unsigned val0, unsigned val1)
{
	if (unlikely((long)p & 4)) {
		p[0] = val1; // start at odd pixel
		++p; --n;
	}
	unsigned tmp[2] = { val0, val1 };
	asm volatile (
		"movq   (%0), %%mm0;"
		: // no output
		: "r" (tmp)
		#ifdef __MMX__
		: "mm0"
		#endif
	);
	unsigned* e = p + n - 7;
	for (/**/; p < e; p += 8) {
		asm volatile (
			"movq %%mm0,   (%0);"
			"movq %%mm0,  8(%0);"
			"movq %%mm0, 16(%0);"
			"movq %%mm0, 24(%0);"
			: // no output
			: "r" (p)
		);
	}
	if (unlikely(n & 4)) {
		asm volatile (
			"movq %%mm0,  (%0);"
			"movq %%mm0, 8(%0);"
			: // no output
			: "r" (p)
		);
		p += 4;
	}
	if (unlikely(n & 2)) {
		asm volatile (
			"movq %%mm0, (%0);"
			: // no output
			: "r" (p)
		);
		p += 2;
	}
	if (unlikely(n & 1)) {
		p[0] = val0;
	}
	asm volatile ( "emms" );
}

static inline void memset4_2_SSE(
	unsigned* p, unsigned n, unsigned val0, unsigned val1)
{
	if (unlikely((long)p & 4)) {
		p[0] = val1; // start at odd pixel
		++p; --n;
	}
	if (likely(n >= 4)) {
		if (unlikely((long)p & 8)) {
			// SSE *must* have 16-byte aligned data
			p[0] = val0;
			p[1] = val1;
			p += 2; n -= 2;
		}
		asm volatile (
			// TODO check this
			"movss        %0, %%xmm0;"
			"movss        %1, %%xmm1;"
			"unpcklps %%xmm0, %%xmm0;"
			"unpcklps %%xmm1, %%xmm1;"
			"unpcklps %%xmm1, %%xmm0;"
			: // no output
			: "m" (val0), "m" (val1)
			#ifdef __SSE__
			: "xmm0", "xmm1"
			#endif
		);
		unsigned* e = p + n - 7;
		for (/**/; p < e; p += 8) {
			asm volatile (
				"movaps %%xmm0,   (%0);"
				"movaps %%xmm0, 16(%0);"
				: // no output
				: "r" (p)
			);
		}
		if (unlikely(n & 4)) {
			asm volatile (
				"movaps %%xmm0, (%0);"
				: // no output
				: "r" (p)
			);
			p += 4;
		}
	}
	if (unlikely(n & 2)) {
		p[0] = val0;
		p[1] = val1;
		p += 2;
	}
	if (unlikely(n & 1)) {
		p[0] = val0;
	}
}

static inline void memset4_2_SSE_s(
	unsigned* p, unsigned n, unsigned val0, unsigned val1)
{
	if (unlikely((long)p & 4)) {
		p[0] = val1; // start at odd pixel
		++p; --n;
	}
	if (likely(n >= 4)) {
		if (unlikely((long)p & 8)) {
			// SSE *must* have 16-byte aligned data
			p[0] = val0;
			p[1] = val1;
			p += 2; n -= 2;
		}
		asm volatile (
			// TODO check this
			"movss        %0, %%xmm0;"
			"movss        %1, %%xmm1;"
			"unpcklps %%xmm0, %%xmm0;"
			"unpcklps %%xmm1, %%xmm1;"
			"unpcklps %%xmm1, %%xmm0;"
			: // no output
			: "m" (val0), "m" (val1)
			#ifdef __SSE__
			: "xmm0", "xmm1"
			#endif
		);
		unsigned* e = p + n - 7;
		for (/**/; p < e; p += 8) {
			asm volatile (
				"movntps %%xmm0,   (%0);"
				"movntps %%xmm0, 16(%0);"
				: // no output
				: "r" (p)
			);
		}
		if (unlikely(n & 4)) {
			asm volatile (
				"movntps %%xmm0, (%0);"
				: // no output
				: "r" (p)
			);
			p += 4;
		}
	}
	if (unlikely(n & 2)) {
		p[0] = val0;
		p[1] = val1;
		p += 2;
	}
	if (unlikely(n & 1)) {
		p[0] = val0;
	}
}


template<bool STREAMING> static inline void memset_2_helper(
	unsigned* out, unsigned num, unsigned val0, unsigned val1)
{
	assert(((long)out & 3) == 0); // must be 4-byte aligned

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasMMXEXT()) {
		if (STREAMING) {
			memset4_2_SSE_s(out, num, val0, val1);
		} else {
			memset4_2_SSE(out, num, val0, val1);
		}
		return;
	}
	if (cpu.hasMMX()) {
		memset4_2_MMX(out, num, val0, val1);
		return;
	}
	#endif

	memset4_2_CPP(out, num, val0, val1);
}

template<bool STREAMING> static inline void memset_2_helper(
	word* out, unsigned num, word val0, word val1)
{
	if (unlikely((long)out & 2)) {
		out[0] = val1;
		++out; --num;
	}

	unsigned val = OPENMSX_BIGENDIAN
	             ? (val0 << 16) | val1
	             : val0 | (val1 << 16);
	memset_2<unsigned, STREAMING>((unsigned*)out, num / 2, val, val);
	if (num & 1) {
		out[num - 1] = val0;
	}
}

template <typename Pixel, bool STREAMING>
void memset_2(Pixel* out, unsigned num, Pixel val0, Pixel val1)
{
	// partial specialization of function templates is not allowed,
	// so instead use function overloading
	memset_2_helper<STREAMING>(out, num, val0, val1);
}
template <typename Pixel, bool STREAMING>
void memset(Pixel* out, unsigned num, Pixel val)
{
	memset_2<Pixel, STREAMING>(out, num, val, val);
}


// Force template instantiation
template void memset_2<unsigned,  true>(unsigned*, unsigned, unsigned, unsigned);
template void memset_2<unsigned, false>(unsigned*, unsigned, unsigned, unsigned);
template void memset_2<word    ,  true>(word*,     unsigned, word    , word    );
template void memset_2<word    , false>(word*    , unsigned, word    , word    );
template void memset  <unsigned,  true>(unsigned*, unsigned, unsigned);
template void memset  <unsigned, false>(unsigned*, unsigned, unsigned);
template void memset  <word    ,  true>(word*,     unsigned, word    );
template void memset  <word    , false>(word*    , unsigned, word    );



void stream_memcpy(unsigned* dst, const unsigned* src, unsigned num)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasMMXEXT()) {
		assert(((long)src & 3) == 0); // must be word aligned
		assert(((long)dst & 3) == 0);
		// align on 8-byte boundary
		if (unlikely((long)dst & 4)) {
			*dst++ = *src++;
			--num;
		}
		// copy chunks of 64 bytes
		unsigned n2 = num & ~15;
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
				"addl  $64, %2;"
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
	memcpy(dst, src, num * sizeof(unsigned));
}

void stream_memcpy(word* dst, const word* src, unsigned num)
{
	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if (cpu.hasMMXEXT()) {
		// align on 4-byte boundary
		if (unlikely((long)dst & 2)) {
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
#ifdef HAVE_POSIX_MEMALIGN
	void* aligned;
	posix_memalign(&aligned, alignment, size);
	#ifdef DEBUG
	AllocMap::instance().insert(aligned, aligned);
	#endif
	return aligned;
#else
	unsigned long t = alignment - 1;
	void* unaligned = malloc(size + t);
	void* aligned = (void*)(((unsigned long)unaligned + t) & ~t);
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
