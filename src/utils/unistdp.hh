#ifndef UNISTDP_HH
#define UNISTDP_HH

#ifndef _MSC_VER

#include <unistd.h>

#else

#include <process.h>
#include <direct.h>

#define getpid _getpid

using mode_t = int;

#endif

#endif // UNISTDP_HH
