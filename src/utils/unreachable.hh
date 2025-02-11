#ifndef UNREACHABLE_HH
#define UNREACHABLE_HH

#include <utility>

#if defined(NDEBUG)
  #define UNREACHABLE std::unreachable()

#else
  // asserts enabled
  // One some platforms, like MinGW, the compiler cannot determine that
  // assert(false) will never return, so we help it by wrapping the assert
  // in an infinite loop.
  #include <cassert>
  #define UNREACHABLE while (1) assert(false)

#endif

#endif // UNREACHABLE_HH
