// $Id$

#ifndef OPENMSX_HH
#define OPENMSX_HH

// don't just always include this, saves about 1 minute build time!!
#ifdef DEBUG
#include <iostream>
#include <sstream>
#endif

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
typedef signed char signed_byte;
/** 8 bit unsigned integer */
typedef unsigned char byte;

/** 16 bit signed integer */
typedef short signed_word;
/** 16 bit unsigned integer */
typedef unsigned short word;

/** 32 bit signed integer */
typedef int int32;
/** 32 bit unsigned integer */
typedef unsigned uint32;

/** 64 bit signed integer */
typedef long long int64;
/** 64 bit unsigned integer */
typedef unsigned long long uint64;

#ifdef DEBUG

#ifdef _WIN32

void DebugPrint(const char* output);

#define PRT_DEBUG(mes)				\
	do {					\
		std::ostringstream output;			\
		output << mes;						\
		std::cout << output << std::endl;	\
		DebugPrint(output.str().c_str());	\
	} while (0)
#else
#define PRT_DEBUG(mes)				\
	do {					\
		std::cout << mes << std::endl;	\
	} while (0)
#endif
#else

#define PRT_DEBUG(mes)

#endif

} // namespace openmsx

#endif
