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
	RawFrame(const SDL_PixelFormat* format, unsigned maxWidth, unsigned height);
	virtual ~RawFrame();

	virtual unsigned getLineBufferSize() const;
	virtual unsigned getLineWidth(unsigned line);

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidth[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		assert(line < getHeight());
		Pixel* dummy = 0;
		Pixel* pixels = getLinePtr(line, dummy);
		pixels[0] = color;
		lineWidth[line] = 1;
	}

	virtual unsigned getRowLength() const;

protected:
	virtual void* getLinePtrImpl(unsigned line);
	virtual bool hasContiguousStorage() const;

private:
	char* data;
	unsigned* lineWidth;
	unsigned maxWidth;
	unsigned pitch;
};

} // namespace openmsx

#endif
