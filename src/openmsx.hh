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

/** 8 bit unsigned integer */
using byte = uint8_t;

/** 16 bit unsigned integer */
using word = uint16_t;

} // namespace openmsx

#endif
