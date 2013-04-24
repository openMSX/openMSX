#ifndef CSTDLIBP_HH
#define CSTDLIBP_HH

#include <cstdlib>

#ifdef _MSC_VER
#define strtoll  _strtoi64
#define strtoull _strtoui64
#endif

#endif
