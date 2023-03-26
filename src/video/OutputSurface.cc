#include "OutputSurface.hh"
#include "endian.hh"

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

} // namespace openmsx
