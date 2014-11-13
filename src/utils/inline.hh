#ifndef INLINE_HH
#define INLINE_HH

#ifdef __GNUC__
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE __attribute__((noinline))

#elif defined _MSC_VER && 0
// Enabling these macros appears to make openmsx about 5% slower
// when compiled with VC++ for x64. Hence we leave the defaults
#define ALWAYS_INLINE __forceinline
#define NEVER_INLINE __declspec(noinline)

#else
// Fallback
#define ALWAYS_INLINE inline
#define NEVER_INLINE

#endif

#endif
