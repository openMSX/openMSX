#ifndef UNREACHABLE_HH
#define UNREACHABLE_HH

// TODO use c++23 std::unreachable()

// Clang has a very convenient way of testing features, unfortunately (for now)
// it's clang-only so add a fallback for non-clang compilers.
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if defined(NDEBUG)
  // clang
  #if __has_builtin(__builtin_unreachable)
    #define UNREACHABLE __builtin_unreachable()

  // __builtin_unreachable() was introduced in gcc-4.5, but 4.5 and 4.6 contain
  // bugs on architectures with delay slots (e.g. MIPS). Look at the git
  // history of this file for more details.
  #elif ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7)))
    // gcc-4.7 or above
    #define UNREACHABLE __builtin_unreachable()

  #elif defined(_MSC_VER)
    // visual studio
    #define UNREACHABLE __assume(0)

  #else
    // fall-back
    #define UNREACHABLE /*nothing*/

  #endif

#else
  // asserts enabled
  // One some platforms, like MinGW, the compiler cannot determine that
  // assert(false) will never return, so we help it by wrapping the assert
  // in an infinite loop.
  #include <cassert>
  #define UNREACHABLE while (1) assert(false)

#endif

#endif // UNREACHABLE_HH
