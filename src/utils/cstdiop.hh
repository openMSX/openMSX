// $Id$

#ifndef CSTDIOP_HH
#define CSTDIOP_HH

#include <cstdio>

#ifdef _MSC_VER
#include <io.h>

#define STDIN_FILENO _fileno(stdin)
#define read _read
#define snprintf _snprintf
#endif

#endif
