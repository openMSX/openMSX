#ifndef INLINE_HH
#define INLINE_HH

// This inline trick doesn't work on gcc 3.x when using debug flavour
// so we only enable it in the other cases.

#if __GNUC__ > 3 || (!defined(DEBUG) && __GNUC__ > 2)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE __attribute__((noinline))
#elif defined _MSC_VER && 0
// Enabling these macros appears to make openmsx about 5% slower
// when compiled with VC++ for x64. Hence we leave the defaults
#define ALWAYS_INLINE __forceinline
#define NEVER_INLINE __declspec(noinline)
#else
#define ALWAYS_INLINE inline
#define NEVER_INLINE
#endif

#endif
