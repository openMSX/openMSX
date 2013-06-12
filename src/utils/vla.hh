#ifndef VLA_HH
#define VLA_HH

// VLAs (Variable Length Array's) are part of C99, but not of C++98.
// Though gcc does support VLAs as an extension. For VC++ we have to emulate
// them via alloca.

#ifndef _MSC_VER

#define VLA(TYPE, NAME, LENGTH) \
	TYPE NAME[(LENGTH)]

#if defined __i386 || defined __x86_64
#define VLA_ALIGNED(TYPE, NAME, LENGTH, ALIGNMENT) \
	TYPE NAME[(LENGTH)] __attribute__((__aligned__((ALIGNMENT))))
#else
// Except on x86/x86-64, GCC can align VLAs within a stack frame, but it makes
// no guarantees if the requested alignment is larger than the alignment of the
// stack frame itself.
//   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=16660
#define VLA_ALIGNED(TYPE, NAME, LENGTH, ALIGNMENT) \
	UNABLE_TO_GUARANTEE_VLA_ALIGNMENT_ON_THIS_ARCHITECTURE
#endif

#else

#define VLA(TYPE, NAME, LENGTH) \
	auto NAME = static_cast<TYPE*>(_alloca(sizeof(TYPE) * (LENGTH)))

// mfeingol: evil hack alert
#define VLA_ALIGNED(TYPE, NAME, LENGTH, ALIGNMENT) \
	size_t cbAlign##NAME = (ALIGNMENT); \
	void* palloc##NAME = _alloca(sizeof(TYPE) * (LENGTH) + cbAlign##NAME); \
	palloc##NAME = (void*)((size_t(palloc##NAME) + cbAlign##NAME - 1UL) & ~(cbAlign##NAME - 1UL)); \
	auto NAME = static_cast<TYPE*>(palloc##NAME); \

#endif

// Macro to align a buffer that might be used by SSE instructions.
// On platforms without SSE no (extra) alignment is performed.
#ifdef __SSE2__
#define VLA_SSE_ALIGNED(TYPE, NAME, LENGTH) VLA_ALIGNED(TYPE, NAME, LENGTH, 16)
#else
#define VLA_SSE_ALIGNED(TYPE, NAME, LENGTH) VLA(TYPE, NAME, LENGTH)
#endif

#endif // VLA_HH
