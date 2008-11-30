// $Id$

#ifndef INLINE_HH
#define INLINE_HH

// This inline trick doesn't work on gcc 3.x when using debug flavour
// so we only enable it in the other cases.

#if __GNUC__ > 3 || (!defined(DEBUG) && __GNUC__ > 2)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE __attribute__((noinline))
#else
#define ALWAYS_INLINE inline
#define NEVER_INLINE
#endif

#endif
