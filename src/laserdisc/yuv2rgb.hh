// $Id$

#ifndef YUV2RGB_HH
#define YUV2RGB_HH

#include <theora/theora.h>

namespace openmsx {

class RawFrame;

namespace yuv2rgb {

void convert(const yuv_buffer& input, RawFrame& output);

} // namespace yuv2rgb
} // namespace openmsx

#endif
