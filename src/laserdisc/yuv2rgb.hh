#ifndef YUV2RGB_HH
#define YUV2RGB_HH

#include <theora/theoradec.h>

namespace openmsx {

class RawFrame;

namespace yuv2rgb {

void convert(const th_ycbcr_buffer& input, RawFrame& output);

} // namespace yuv2rgb
} // namespace openmsx

#endif
