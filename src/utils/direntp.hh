#ifndef DIRENTP_HH
#define DIRENTP_HH

#ifndef _WIN32
#include <dirent.h>
#else
// When building with MinGW, we use our own wrapper instead of the
// native dirent.h provided by the MinGW C runtime. We do this because
// we need an implementation of dirent that returns UTF8-encoded strings
// as chars.
// Unfortunately, MinGW's implementation of dirent provides either
// wchar_t-based versions that support UTF16 (when UNICODE is defined),
// or char-based versions containing what appear to be ANSI strings,
// certainly not UTF8.
// While this behavior is relatively consistent with Win32, it's not
// what we need here. Consequently, we use our wrapper instead.
#include "win32-dirent.hh"
#endif

#endif
