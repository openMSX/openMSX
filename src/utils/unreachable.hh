#ifndef UNREACHABLE_HH
#define UNREACHABLE_HH

// GCC targeting MIPS will generate bad code when marking certain code as
// unreachable. This is very noticeable in SDLVideoSystem::getWindowSize(),
// which does not seem to write the output arguments, making openMSX fail the
// creation of an SDL video mode since the requested size is rubbish.
//
// This compiler bug has been reported as:
//    http://gcc.gnu.org/bugzilla/show_bug.cgi?id=51861
//
// It turns out this was not a specific issue for MIPS, but it could affect all
// architectures which use branch delay slots (e.g. also AVR and SPARC).
//
// At this moment the bug in gcc has been fixed, but the most recent gcc-4.5.x
// and gcc-4.6.x releases still contain the bug.
//
// For openMSX, this UNREACHABLE-optimization is not very important (it does
// improve speed/code-size, but not in very critical locations). So we decided
// to only enable this optimization starting from gcc-4.7 or from gcc-4.5 for a
// few white-listed architectures that we know don't use delay slots.

#if defined(NDEBUG)
  // __builtin_unreachable() was introduced in gcc-4.5
  #if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7)))
    // gcc-4.7 or above
    #define UNREACHABLE __builtin_unreachable()

  #elif ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 5) && (defined(__i386__) || defined(__x86_64__) || defined(__arm__)))
    // gcc-4.5 or gcc-4.6 on x86, x86_64 or arm (all without delay slots)
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
  #include <cassert>
  #define UNREACHABLE assert(false)

#endif

#endif // UNREACHABLE_HH
