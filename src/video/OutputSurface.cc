#include "OutputSurface.hh"
#include "build-info.hh"

namespace openmsx {

void OutputSurface::calculateViewPort(gl::ivec2 logSize_, gl::ivec2 physSize_)
{
	m_logicalSize = logSize_;
	m_physSize = physSize_;

	gl::vec2 logSize(logSize_); // convert int->float
	gl::vec2 physSize(physSize_);

	float scale = min_component(physSize / logSize);
	m_viewScale = gl::vec2(scale); // for now always same X and Y scale

	gl::vec2 viewSize = logSize * scale;
	m_viewSize = round(viewSize);

	gl::vec2 viewOffset = (physSize - viewSize) / 2.0f;
	m_viewOffset = round(viewOffset);
}

void OutputSurface::setOpenGlPixelFormat()
{
	setPixelFormat(PixelFormat(
		32,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x00FF0000, OPENMSX_BIGENDIAN ? 24 : 16, 0,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00, OPENMSX_BIGENDIAN ? 16 :  8, 0,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x000000FF, OPENMSX_BIGENDIAN ?  8 :  0, 0,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000, OPENMSX_BIGENDIAN ?  0 : 24, 0));
}

} // namespace openmsx
