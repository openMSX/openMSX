#ifndef STRINGSP_HH
#define STRINGSP_HH

#ifndef _MSC_VER
#include <strings.h>
#else

#include <string.h>
#define strncasecmp _strnicmp

#endif

#endif
