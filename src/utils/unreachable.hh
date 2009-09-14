// $Id$

#ifndef UNREACHABLE_HH
#define UNREACHABLE_HH

#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5))) && defined(NDEBUG)

// __builtin_unreachable() was introduced in gcc-4.5
#define UNREACHABLE __builtin_unreachable()

#else

// pre gcc-4.5 (or non-gcc compiler) or asserts enabled,
#include <cassert>
#define UNREACHABLE assert(false)

#endif

#endif // UNREACHABLE_HH
