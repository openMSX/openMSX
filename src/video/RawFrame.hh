// $Id$

#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"
#include <cassert>

namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame : public FrameSource
{
public:
	RawFrame(const SDL_PixelFormat* format, unsigned bytesPerPixel,
	         unsigned maxWidth, unsigned height);
	~RawFrame();

	virtual unsigned getLineBufferSize() const;
	virtual unsigned getLineWidth(unsigned line);

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidth[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		setBlank(line, color, color);
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel colour0, Pixel colour1) {
		assert(line < getHeight());
		Pixel* pixels = getLinePtr(line, (Pixel*)0);
		pixels[0] = colour0;
		pixels[1] = colour1; // TODO: We store colour1, but no-one uses it.
		lineWidth[line] = 1;
	}

protected:
	virtual void* getLinePtrImpl(unsigned line);

private:
	unsigned maxWidth;
	unsigned* lineWidth;
	unsigned pitch;
	char* data;
};

} // namespace openmsx

#endif // RAWFRAME_HH
