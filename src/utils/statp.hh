#ifndef STATP_HH
#define STATP_HH

#include <sys/stat.h>

#ifdef _MSC_VER
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#endif
