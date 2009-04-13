// $Id$

#ifndef UNISTDP_HH
#define UNISTDP_HH

#include "probed_defs.hh"

#ifndef _MSC_VER

#include <unistd.h>

#else

#include <process.h>
#include <direct.h>

#define getpid _getpid

typedef int mode_t;

#endif

#endif // UNISTDP_HH
