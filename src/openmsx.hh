#ifndef OPENMSX_HH
#define OPENMSX_HH

#include "build-info.hh"

// don't just always include this, saves about 1 minute build time!!
#ifdef DEBUG
#include <iostream>
#include <sstream>
#endif

#if PLATFORM_ANDROID
#include <android/log.h>
#define ad_printf(...) __android_log_print(ANDROID_LOG_INFO, "openMSX", __VA_ARGS__)
#else
#define ad_printf(...)
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

#ifdef DEBUG

#ifdef _WIN32

void DebugPrint(const char* output);

#define PRT_DEBUG(mes)				\
	do {					\
		std::ostringstream output;			\
		output << mes;						\
		std::cout << output << std::endl;	\
		::openmsx::DebugPrint(output.str().c_str());	\
	} while (0)
#elif PLATFORM_ANDROID
#define PRT_DEBUG(mes)				\
	do {					\
		std::ostringstream output;			\
		output << mes;						\
		std::cout << output << std::endl;	\
		__android_log_write(ANDROID_LOG_DEBUG, "openMSX", output.str().c_str()); \
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


#if defined(__GNUC__) && \
    ((__GNUC__ * 100 + __GNUC_MINOR__ * 10 + __GNUC_PATCHLEVEL__) < 472)
	// gcc versions before 4.7.2 had a bug in ~unique_ptr(),
	// see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54351
	#define UNIQUE_PTR_BUG 1
#else
	#define UNIQUE_PTR_BUG 0
#endif

} // namespace openmsx

#endif
