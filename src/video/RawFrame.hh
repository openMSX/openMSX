#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"
#include "MemBuffer.hh"
#include <cassert>

namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame : public FrameSource
{
public:
	RawFrame(const SDL_PixelFormat& format, unsigned maxWidth, unsigned height);
	virtual ~RawFrame();

	template<typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidths[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		assert(line < getHeight());
		Pixel* pixels = getLinePtrDirect<Pixel>(line);
		pixels[0] = color;
		lineWidths[line] = 1;
	}

	virtual unsigned getRowLength() const;

protected:
	virtual unsigned getLineWidth(unsigned line) const;
	virtual const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const;
	virtual bool hasContiguousStorage() const;

private:
	char* data;
	MemBuffer<unsigned> lineWidths;
	unsigned maxWidth;
	unsigned pitch;
};

} // namespace openmsx

#endif
