// $Id$

#ifndef OPENMSX_HH
#define OPENMSX_HH

#include <iostream>

/// Namespace of the openMSX emulation core.
/** openMSX: the MSX emulator that aims for perfection
  *
  * Copyrights: see AUTHORS file.
  * License: GPL.
  */
namespace openmsx {

/** 4 bit integer */
typedef unsigned char nibble;

/** 8 bit signed integer */
typedef char signed_byte;
/** 8 bit unsigned integer */
typedef unsigned char byte;

/** 16 bit signed integer */
typedef short signed_word;
/** 16 bit unsigned integer */
typedef unsigned short word;

/** 32 bit unsigned integer */
typedef unsigned int uint32;

#ifdef _WIN32
#ifdef	__MINGW32__
#include <_mingw.h>	// for __int64
#endif
#define	uint	unsigned int
// Use define instead of typedef. See _mingw.h.
/** 64 bit signed integer */
#define	int64	__int64
/** 64 bit unsigned integer */
#define	uint64	unsigned __int64
#else
// this is not portable to 64bit platforms? -> TODO check
/** 64 bit signed integer */
typedef long long int64;
// this is not portable to 64bit platforms? -> TODO check
/** 64 bit unsigned integer */
typedef unsigned long long uint64;
#endif

#ifdef DEBUGVAL
#undef DEBUGVAL
#endif
#ifdef DEBUG
#define DEBUGVAL 1
#else
#define DEBUGVAL 0
#endif

#define PRT_DEBUG(mes)				\
	do {					\
		if (DEBUGVAL) {			\
			std::cout << mes << std::endl;	\
		}				\
	} while (0)

} // namespace openmsx

#endif
