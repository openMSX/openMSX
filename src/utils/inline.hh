// $Id$

#ifndef INLINE_HH
#define INLINE_HH

#if __GNUC__ > 2
#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NEVER_INLINE __attribute__((noinline))
#else
#define ALWAYS_INLINE inline
#define NEVER_INLINE
#endif

#endif
