#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"
#include "MemBuffer.hh"
#include <cassert>
#include <concepts>

namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame final : public FrameSource
{
public:
	RawFrame(const PixelFormat& format, unsigned maxWidth, unsigned height);

	template<std::unsigned_integral Pixel>
	[[nodiscard]] std::span<Pixel> getLineDirect(unsigned y) {
		assert(y < getHeight());
		return {reinterpret_cast<Pixel*>(data.data() + y * pitch), maxWidth};
	}

	[[nodiscard]] unsigned getLineWidthDirect(unsigned y) const {
		assert(y < getHeight());
		return lineWidths[y];
	}

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidths[line] = width;
	}

	template<std::unsigned_integral Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		assert(line < getHeight());
		getLineDirect<Pixel>(line)[0] = color;
		lineWidths[line] = 1;
	}

protected:
	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;
	[[nodiscard]] bool hasContiguousStorage() const override;

private:
	MemBuffer<char, 64> data;
	MemBuffer<unsigned> lineWidths;
	unsigned maxWidth;
	unsigned pitch;
};

} // namespace openmsx

#endif
