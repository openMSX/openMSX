// $Id$

#ifndef YUV2RGB_HH
#define YUV2RGB_HH

#define OGG_SUPPORT

#ifdef OGG_SUPPORT

#include <openmsx.hh>
#include <theora/theora.h>

namespace openmsx {
namespace yuv2rgb {

void convert(byte *rawFrame, const yuv_buffer *buffer);

} // namespace yuv2rgb
} // namespace openmsx

#endif

#endif
