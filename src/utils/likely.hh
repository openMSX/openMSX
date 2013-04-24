#ifndef LIKELY_HH
#define LIKELY_HH

/* Somewhere in the middle of the GCC 2.96 development cycle, we implemented
 * a mechanism by which the user can annotate likely branch directions and
 * expect the blocks to be reordered appropriately.  Define __builtin_expect
 * to nothing for earlier compilers.
 */

#if __GNUC__ > 2
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#endif
