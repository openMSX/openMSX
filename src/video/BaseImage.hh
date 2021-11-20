#ifndef BASEIMAGE_HH
#define BASEIMAGE_HH

#include "gl_vec.hh"
#include <cstdint>

namespace openmsx {

class OutputSurface;

class BaseImage
{
public:
	/**
	 * Performs a sanity check on image size.
	 * Throws MSXException if width or height is excessively large.
	 * Negative image sizes are valid and flip the image.
	 */
	static void checkSize(gl::ivec2 size);

	virtual ~BaseImage() = default;
	virtual void draw(OutputSurface& output, gl::ivec2 pos,
	                  uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) = 0;
	[[nodiscard]] gl::ivec2 getSize() const { return size; }

	void draw(OutputSurface& output, gl::ivec2 pos, uint8_t alpha = 255) {
		draw(output, pos, 255, 255, 255, alpha);
	}

protected:
	gl::ivec2 size;
};

} // namespace openmsx

#endif
