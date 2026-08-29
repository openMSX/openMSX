#ifndef OUTPUTDIMENSIONS_HH
#define OUTPUTDIMENSIONS_HH

#include "gl_vec.hh"

namespace openmsx {

class OutputDimensions
{
public:
	OutputDimensions() : OutputDimensions({640, 480}, {640, 480}) {}
	OutputDimensions(gl::ivec2 physSize_, gl::ivec2 viewSize_)
		: m_physSize(physSize_)
		, m_viewSize(viewSize_)
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
