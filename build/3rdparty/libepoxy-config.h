/* Hand-written replacement for the config.h that libepoxy's own build system
 * generates, for use by the Visual C++ build (build/3rdparty/libepoxy.vcxproj).
 *
 * On Windows libepoxy only ever uses WGL: dispatch_common.h hardcodes
 * PLATFORM_HAS_WGL to 1 there and takes EGL and GLX from the values below.
 * ENABLE_X11 is only consulted when EGL is enabled, so it follows ENABLE_EGL.
 *
 * "inline" is a C++-only keyword in MSVC, hence the __inline mapping; this
 * mirrors what libepoxy's meson build does for the msvc compiler.
 */

#ifndef OPENMSX_LIBEPOXY_CONFIG_H
#define OPENMSX_LIBEPOXY_CONFIG_H

#define ENABLE_EGL 0
#define ENABLE_GLX 0
#define ENABLE_X11 0

#define inline __inline

#endif /* OPENMSX_LIBEPOXY_CONFIG_H */
