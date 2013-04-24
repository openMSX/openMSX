#ifndef BASEIMAGE_HH
#define BASEIMAGE_HH

#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class OutputSurface;

class BaseImage : private noncopyable
{
public:
	/**
	 * Performs a sanity check on image size.
	 * Throws MSXException if width or height is excessively large.
	 * Negative image sizes are valid and flip the image.
	 */
	static void checkSize(int width, int height);

	virtual ~BaseImage() {}
	virtual void draw(OutputSurface& output, int x, int y,
	                  byte alpha = 255) = 0;
	virtual int getWidth() const = 0;
	virtual int getHeight() const = 0;
};

} // namespace openmsx

#endif
