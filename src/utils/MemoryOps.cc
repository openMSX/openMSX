#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "likely.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include "systemfuncs.hh"
#include "Math.hh"
#include "unreachable.hh"
#include <type_traits>
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

static inline void memset_128_SSE(uint64_t* dest, size_t num)
{
	assert((size_t(dest) & 15) == 0); // must be 16-byte aligned
	uint64_t* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
#if defined _MSC_VER
		__asm {
			mov			eax,dest
			movaps		xmmword ptr [eax],xmm0
			movaps		xmmword ptr [eax+10h],xmm0
		}
#else
		asm volatile (
			"movaps %%xmm0,   (%[OUT]);"
			"movaps %%xmm0, 16(%[OUT]);"
			: // no output
			: [OUT] "r" (dest)
			: "memory"
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
			"movaps %%xmm0, (%[OUT]);"
			: // no output
			: [OUT] "r" (dest)
			: "memory"
		);
#endif
	}
}

static inline void memset_64_SSE(
	uint64_t* dest, size_t num, uint64_t val)
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
		"movd     %[VAL], %%xmm0;"
		"unpcklps %%xmm0, %%xmm0;"
		: // no output
		: [VAL] "r" (val)
		#if defined __SSE__
		: "xmm0"
		#endif
	);
#else
	uint32_t _low_  = uint32_t(val >>  0);
	uint32_t _high_ = uint32_t(val >> 32);
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
		"movss    %[LOW], %%xmm0;"
		"movss   %[HIGH], %%xmm1;"
		"unpcklps %%xmm0, %%xmm0;"
		"unpcklps %%xmm1, %%xmm1;"
		"unpcklps %%xmm1, %%xmm0;"
		: // no output
		: [LOW]  "m" (_low_)
		, [HIGH] "m" (_high_)
		#if defined __SSE__
		: "xmm0", "xmm1"
		#endif
	);
#endif
#endif
	memset_128_SSE(dest, num);
	if (unlikely(num & 1)) {
		dest[num - 1] = val;
	}
}

static inline void memset_64_MMX(
	uint64_t* dest, size_t num, uint64_t val)
{
    assert((size_t(dest) & 7) == 0); // must be 8-byte aligned

#if defined _MSC_VER
	uint32_t lo = uint32_t(val >> 0);
	uint32_t hi = uint32_t(val >> 32);
	uint64_t* e = dest + num - 3;

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
		"movd     %[LOW],%%mm0;"
		"movd    %[HIGH],%%mm1;"
		"punpckldq %%mm1,%%mm0;"
		: // no output
		: [LOW]  "r" (uint32_t(val >>  0))
		, [HIGH] "r" (uint32_t(val >> 32))
		#if defined __MMX__
		: "mm0", "mm1"
		#endif
	);
	uint64_t* e = dest + num - 3;
	for (/**/; dest < e; dest += 4) {
		asm volatile (
			"movq %%mm0,   (%[OUT]);"
			"movq %%mm0,  8(%[OUT]);"
			"movq %%mm0, 16(%[OUT]);"
			"movq %%mm0, 24(%[OUT]);"
			: // no output
			: [OUT] "r" (dest)
			: "memory"
		);
	}
	if (unlikely(num & 2)) {
		asm volatile (
			"movq %%mm0,  (%[OUT]);"
			"movq %%mm0, 8(%[OUT]);"
			: // no output
			: [OUT] "r" (dest)
			: "memory"
		);
		dest += 2;
	}
	if (unlikely(num & 1)) {
		asm volatile (
			"movq %%mm0, (%[OUT]);"
			: // no output
			: [OUT] "r" (dest)
			: "memory"
		);
	}
	asm volatile ("emms");
#endif
}
#endif

static inline void memset_64(
        uint64_t* dest, size_t num, uint64_t val)
{
	assert((size_t(dest) & 7) == 0); // must be 8-byte aligned

#if ASM_X86 && !defined _WIN64
	if (HostCPU::hasSSE()) {
		memset_64_SSE(dest, num, val);
		return;
	}
	if (HostCPU::hasMMX()) {
		memset_64_MMX(dest, num, val);
		return;
	}
#endif
	uint64_t* e = dest + num - 3;
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

static inline void memset_32_2(
	uint32_t* dest, size_t num, uint32_t val0, uint32_t val1)
{
	assert((size_t(dest) & 3) == 0); // must be 4-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(size_t(dest) & 4)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num;
	}

	uint64_t val = OPENMSX_BIGENDIAN
		? (static_cast<uint64_t>(val0) << 32) | val1
		: val0 | (static_cast<uint64_t>(val1) << 32);
	memset_64(reinterpret_cast<uint64_t*>(dest), num / 2, val);

	if (unlikely(num & 1)) {
		dest[num - 1] = val0;
	}
}

static inline void memset_32(uint32_t* dest, size_t num, uint32_t val)
{
	assert((size_t(dest) & 3) == 0); // must be 4-byte aligned

#if ASM_X86
#if defined _MSC_VER
	// VC++'s __stosd intrinsic results in emulator benchmarks
	// running about 7% faster than with memset_32_2, streaming or not,
	// and about 3% faster than the C code below.
	__stosd(reinterpret_cast<unsigned long*>(dest), val, num);
#else
	memset_32_2(dest, num, val, val);
#endif
#elif defined __arm__
	// Ideally the first mov(*) instruction could be omitted (and then
	// replace 'r3' with '%[val]'. But this can cause problems in the
	// 'stm' instructions when the compiler chooses a register
	// 'bigger' than r4 for [val]. See commit message for LOTS more
	// details.
	asm volatile (
		"mov     r3, %[val]\n\t"  // (*) should not be needed
		"mov     r4, r3\n\t"
		"mov     r5, r3\n\t"
		"mov     r6, r3\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bmi     1f\n"
		"mov     r8, r3\n\t"
		"mov     r9, r3\n\t"
		"mov     r10,r3\n\t"
		"mov     r12,r3\n\t"
	"0:\n\t"
		"stmia   %[dest]!,{r3,r4,r5,r6,r8,r9,r10,r12}\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bpl     0b\n\t"
	"1:\n\t"
		"tst     %[num],#4\n\t"
		"it      ne\n\t"
		"stmne   %[dest]!,{r3,r4,r5,r6}\n\t"
		"tst     %[num],#2\n\t"
		"it      ne\n\t"
		"stmne   %[dest]!,{r3,r4}\n\t"
		"tst     %[num],#1\n\t"
		"it      ne\n\t"
		"strne   r3,[%[dest]]\n\t"

		: [dest] "=r"    (dest)
		, [num] "=r"     (num)
		:       "[dest]" (dest)
		,       "[num]"  (num)
		, [val] "r"      (val)
		: "memory"
		, "r3","r4","r5","r6","r8","r9","r10","r12"
	);
	return;
#else
	uint32_t* e = dest + num - 7;
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

static inline void memset_16_2(
	uint16_t* dest, size_t num, uint16_t val0, uint16_t val1)
{
	assert((size_t(dest) & 1) == 0); // must be 2-byte aligned

	if (unlikely(num == 0)) return;
	if (unlikely(size_t(dest) & 2)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num;
	}

	uint32_t val = OPENMSX_BIGENDIAN
	             ? (val0 << 16) | val1
	             : val0 | (val1 << 16);
	memset_32(reinterpret_cast<uint32_t*>(dest), num / 2, val);

	if (unlikely(num & 1)) {
		dest[num - 1] = val0;
	}
}

static inline void memset_16(uint16_t* dest, size_t num, uint16_t val)
{
	memset_16_2(dest, num, val, val);
}

template<typename Pixel> void MemSet<Pixel>::operator()(
	Pixel* dest, size_t num, Pixel val) const
{
	if (sizeof(Pixel) == 2) {
		memset_16(reinterpret_cast<uint16_t*>(dest), num, val);
	} else if (sizeof(Pixel) == 4) {
		memset_32(reinterpret_cast<uint32_t*>(dest), num, val);
	} else {
		UNREACHABLE;
	}
}

template<typename Pixel> void MemSet2<Pixel>::operator()(
	Pixel* dest, size_t num, Pixel val0, Pixel val1) const
{
	if (sizeof(Pixel) == 2) {
		memset_16_2(reinterpret_cast<uint16_t*>(dest), num, val0, val1);
	} else if (sizeof(Pixel) == 4) {
		memset_32_2(reinterpret_cast<uint32_t*>(dest), num, val0, val1);
	} else {
		UNREACHABLE;
	}
}

// Force template instantiation
template struct MemSet <uint16_t>;
template struct MemSet <uint32_t>;
template struct MemSet2<uint16_t>;
template struct MemSet2<uint32_t>;



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
		auto it = allocMap.find(aligned);
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

void* mallocAligned(size_t alignment, size_t size)
{
	assert("must be a power of 2" && Math::isPowerOfTwo(alignment));
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
	auto t = alignment - 1;
	void* unaligned = malloc(size + t);
	if (!unaligned) {
		throw std::bad_alloc();
	}
	auto aligned = reinterpret_cast<void*>(
		(reinterpret_cast<size_t>(unaligned) + t) & ~t);
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
