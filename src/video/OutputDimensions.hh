#ifndef OUTPUTDIMENSIONS_HH
#define OUTPUTDIMENSIONS_HH

#include "gl_vec.hh"

namespace openmsx {

class OutputDimensions
{
public:
	OutputDimensions() = default;
	OutputDimensions(gl::ivec2 logSize, gl::ivec2 physSize);

	[[nodiscard]] int getLogicalWidth()  const { return m_logicalSize.x; }
	[[nodiscard]] int getLogicalHeight() const { return m_logicalSize.y; }
	[[nodiscard]] gl::ivec2 getLogicalSize()  const { return m_logicalSize; }
	[[nodiscard]] gl::ivec2 getPhysicalSize() const { return m_physSize; }

	[[nodiscard]] gl::ivec2 getViewOffset() const { return m_viewOffset; }
	[[nodiscard]] gl::ivec2 getViewSize()   const { return m_viewSize; }
	[[nodiscard]] gl::vec2  getViewScale()  const { return m_viewScale; }

private:
	gl::ivec2 m_logicalSize;
	gl::ivec2 m_physSize;
	gl::ivec2 m_viewOffset;
	gl::ivec2 m_viewSize;
	gl::vec2 m_viewScale{1.0f};
};

} // namespace openmsx

#endif
