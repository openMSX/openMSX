#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"
#include "MemBuffer.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

// Used by SDLRasterizer to implement left/right border drawing optimization.
struct V9958RasterizerBorderInfo
{
	V9958RasterizerBorderInfo()
		: mode(0xff) {} // invalid mode
	uint32_t color0, color1;
	byte mode, adjust, scroll;
	bool masked;
};


/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame final : public FrameSource
{
public:
	RawFrame(const SDL_PixelFormat& format, unsigned maxWidth, unsigned height);
	virtual ~RawFrame();

	template<typename Pixel>
	Pixel* getLinePtrDirect(unsigned y) {
		return reinterpret_cast<Pixel*>(data + y * pitch);
	}

	unsigned getLineWidthDirect(unsigned y) const {
		return lineWidths[y];
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

	// RawFrame is mostly agnostic of the border info struct. The only
	// thing it does is store the information and give access to it.
	V9958RasterizerBorderInfo& getBorderInfo() { return borderInfo; }

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

	V9958RasterizerBorderInfo borderInfo;
};

} // namespace openmsx

#endif
