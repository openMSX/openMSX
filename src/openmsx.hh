#ifndef OPENMSX_HH
#define OPENMSX_HH

#include "build-info.hh"
#include <cstdint>

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
using nibble = uint8_t;

/** 8 bit unsigned integer */
using byte = uint8_t;

/** 16 bit unsigned integer */
using word = uint16_t;


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
