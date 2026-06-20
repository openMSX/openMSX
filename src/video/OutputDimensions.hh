#ifndef OUTPUTDIMENSIONS_HH
#define OUTPUTDIMENSIONS_HH

#include "gl_vec.hh"

namespace openmsx {

class OutputDimensions
{
public:
	OutputDimensions() : OutputDimensions({640, 480}, 2.0f) {}
	OutputDimensions(gl::ivec2 physSize_, float factor)
		: m_physSize(physSize_)
		, m_viewSize(gl::round(gl::vec2(320.0f, 240.0f) * factor))
		, m_viewOffset((m_physSize - m_viewSize) / 2) {}

	[[nodiscard]] gl::ivec2 getPhysicalSize() const { return m_physSize; }
	[[nodiscard]] gl::ivec2 getViewOffset() const { return m_viewOffset; }
	[[nodiscard]] gl::ivec2 getViewSize()   const { return m_viewSize; }

	[[nodiscard]] bool operator==(const OutputDimensions&) const = default;

private:
	gl::ivec2 m_physSize;
	gl::ivec2 m_viewSize;
	gl::ivec2 m_viewOffset;
};

} // namespace openmsx

#endif
