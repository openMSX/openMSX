#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include "PixelOperations.hh"
#include "gl_vec.hh"

#include <cassert>
#include <cstdint>
#include <string>

namespace openmsx {

/** A frame buffer where pixels can be written to.
  * It could be an in-memory buffer or a video buffer visible to the user
  * (see *OffScreenSurface and *VisibleSurface classes).
  *
  * The OutputSurface class itself knows about its dimensions and pixel format.
  * But the actual pixel storage is left for the subclasses.
  */
class OutputSurface
{
public:
	using Pixel = uint32_t;

	OutputSurface(const OutputSurface&) = delete;
	OutputSurface(OutputSurface&&) = delete;
	OutputSurface& operator=(const OutputSurface&) = delete;
	OutputSurface& operator=(OutputSurface&&) = delete;
	virtual ~OutputSurface() = default;

	[[nodiscard]] int getLogicalWidth()  const { return m_logicalSize.x; }
	[[nodiscard]] int getLogicalHeight() const { return m_logicalSize.y; }
	[[nodiscard]] gl::ivec2 getLogicalSize()  const { return m_logicalSize; }
	[[nodiscard]] gl::ivec2 getPhysicalSize() const { return m_physSize; }

	[[nodiscard]] gl::ivec2 getViewOffset() const { return m_viewOffset; }
	[[nodiscard]] gl::ivec2 getViewSize()   const { return m_viewSize; }
	[[nodiscard]] gl::vec2  getViewScale()  const { return m_viewScale; }

	/** Returns the pixel value for the given RGB color.
	  * No effort is made to ensure that the returned pixel value is not the
	  * color key for this output surface.
	  */
	[[nodiscard]] uint32_t mapRGB(gl::vec3 rgb) const
	{
		return mapRGB255(gl::ivec3(rgb * 255.0f));
	}

	/** Same as mapRGB, but RGB components are in range [0..255].
	 */
	[[nodiscard]] uint32_t mapRGB255(gl::ivec3 rgb) const
	{
		auto [r, g, b] = rgb;
		PixelOperations pixelOps;
		return pixelOps.combine(r, g, b);
	}

	/** Returns the color key for this output surface.
	  */
	[[nodiscard]] Pixel getKeyColor() const
	{
		return 0x00000000; // alpha = 0
	}

	/** Save the content of this OutputSurface to a PNG file.
	  * @throws MSXException If creating the PNG file fails.
	  */
	virtual void saveScreenshot(const std::string& filename) = 0;

protected:
	OutputSurface() = default;

	// These two _must_ be called from (each) subclass constructor.
	void calculateViewPort(gl::ivec2 logSize, gl::ivec2 physSize);

private:
	gl::ivec2 m_logicalSize;
	gl::ivec2 m_physSize;
	gl::ivec2 m_viewOffset;
	gl::ivec2 m_viewSize;
	gl::vec2 m_viewScale{1.0f};
};

} // namespace openmsx

#endif
