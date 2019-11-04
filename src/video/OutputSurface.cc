#include "OutputSurface.hh"
#include "unreachable.hh"

namespace openmsx {

void OutputSurface::calculateViewPort(gl::ivec2 physSize_)
{
	m_physSize = physSize_;
	gl::vec2 physSize(physSize_);

	gl::vec2 logSize = gl::vec2(getLogicalSize());
	float scale = min_component(physSize / logSize);
	m_viewScale = gl::vec2(scale); // for now always same X and Y scale

	gl::vec2 viewSize = logSize * scale;
	m_viewSize = round(viewSize);

	gl::vec2 viewOffset = (physSize - viewSize) / 2.0f;
	m_viewOffset = round(viewOffset);
}

void OutputSurface::flushFrameBuffer()
{
	UNREACHABLE;
}

void  OutputSurface::clearScreen()
{
	UNREACHABLE;
}

} // namespace openmsx
