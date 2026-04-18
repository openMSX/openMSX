#ifndef OUTPUTDIMENSIONS_HH
#define OUTPUTDIMENSIONS_HH

#include "RenderSettings.hh"

#include "gl_vec.hh"

namespace openmsx {

class OutputDimensions
{
public:
	OutputDimensions() = default;
	OutputDimensions(gl::ivec2 physSize, RenderSettings::ScaleMode mode);

	[[nodiscard]] gl::ivec2 getPhysicalSize() const { return m_physSize; }
	[[nodiscard]] gl::ivec2 getViewOffset() const { return m_viewOffset; }
	[[nodiscard]] gl::ivec2 getViewSize()   const { return m_viewSize; }

	[[nodiscard]] bool operator==(const OutputDimensions&) const = default;

private:
	gl::ivec2 m_physSize;
	gl::ivec2 m_viewOffset;
	gl::ivec2 m_viewSize;
};

} // namespace openmsx

#endif
