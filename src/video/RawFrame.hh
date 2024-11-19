#ifndef RAWFRAME_HH
#define RAWFRAME_HH

#include "FrameSource.hh"

#include "MemBuffer.hh"

#include <cassert>

namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame final : public FrameSource
{
public:
	RawFrame(unsigned maxWidth, unsigned height);

	[[nodiscard]] std::span<Pixel> getLineDirect(unsigned y) {
		assert(y < getHeight());
		return data.subspan(y * size_t(maxWidth), maxWidth);
	}
	[[nodiscard]] std::span<const Pixel> getLineDirect(unsigned y) const {
		return const_cast<RawFrame*>(this)->getLineDirect(y);
	}

	[[nodiscard]] unsigned getLineWidthDirect(unsigned y) const {
		assert(y < getHeight());
		return lineWidths[y];
	}

	void setLineWidth(unsigned line, unsigned width) {
		assert(line < getHeight());
		assert(width <= maxWidth);
		lineWidths[line] = width;
	}

	void setBlank(unsigned line, Pixel color) {
		assert(line < getHeight());
		getLineDirect(line)[0] = color;
		lineWidths[line] = 1;
	}

private:
	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] std::span<const Pixel> getUnscaledLine(
		unsigned line, std::span<Pixel> helpBuf) const override;
	[[nodiscard]] bool hasContiguousStorage() const override;

private:
	MemBuffer<Pixel, 64> data; // aligned on cache-lines
	MemBuffer<unsigned> lineWidths;
	unsigned maxWidth; // may be larger (rounded up) than requested in the constructor
};

} // namespace openmsx

#endif
