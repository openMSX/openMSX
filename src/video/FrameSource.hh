#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH

#include "aligned.hh"
#include "narrow.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>

namespace openmsx {

class PixelFormat;

/** Interface for getting lines from a video frame.
  */
class FrameSource
{
public:
	using Pixel = uint32_t;

	/** What role does this frame play in interlacing?
	  */
	enum FieldType {
		/** Interlacing is off for this frame.
		  */
		FIELD_NONINTERLACED,
		/** Interlacing is on and this is an even frame.
		  */
		FIELD_EVEN,
		/** Interlacing is on and this is an odd frame.
		  */
		FIELD_ODD
	};

	/** (Re)initialize an existing FrameSource. This method sets the
	  * Fieldtype and flushes the 'getLinePtr' buffers.
	  */
	void init(FieldType fieldType_) { fieldType = fieldType_; }

	/** Gets the role this frame plays in interlacing.
	  */
	[[nodiscard]] FieldType getField() const {
		return fieldType;
	}

	/** Gets the number of lines in this frame.
	  */
	[[nodiscard]] unsigned getHeight() const {
		return height;
	}

	/** Gets the number of display pixels on the given line.
	  * @return line width (=1 for a vertical border line)
	  */
	[[nodiscard]] virtual unsigned getLineWidth(unsigned line) const = 0;

	/** Get the width of (all) lines in this frame.
	 * This only makes sense when all lines have the same width, so this
	 * methods asserts that all lines actually have the same width. This
	 * is for example not always the case for MSX frames, but it is for
	 * video frames (for superimpose).
	 */
	[[nodiscard]] unsigned getWidth() const {
		assert(height > 0);
		unsigned result = getLineWidth(0);
		for (auto line : xrange(1u, height)) {
			assert(result == getLineWidth(line)); (void)line;
		}
		return result;
	}

	/** Get the (single) color of the given line.
	  * Typically this will be used to get the color of a vertical border
	  * line. But it's fine to call this on non-border lines as well, in
	  * that case the color of the first pixel of the line is returned.
	  */
	[[nodiscard]] inline Pixel getLineColor(unsigned line) const {
		ALIGNAS_SSE std::array<Pixel, 1280> buf; // large enough for widest line
		unsigned width; // not used
		return reinterpret_cast<const Pixel*>(
			getLineInfo(line, width, buf.data(), 1280))[0];
	}

	/** Gets a pointer to the pixels of the given line number.
	  * The line returned is guaranteed to have the given width. If the
	  * original line had a different width the result will be computed in
	  * the provided work buffer. So that buffer should be big enough to
	  * hold the scaled line. This also means the lifetime of the result
	  * is tied to the lifetime of that work buffer. In any case the return
	  * value of this function will point to the line data (some internal
	  * buffer or the work buffer).
	  */
	[[nodiscard]] inline std::span<const Pixel> getLine(int line, std::span<Pixel> buf) const
	{
		line = std::clamp(line, 0, narrow<int>(getHeight() - 1));
		unsigned internalWidth;
		auto* internalData = reinterpret_cast<const Pixel*>(
			getLineInfo(line, internalWidth, buf.data(), narrow<unsigned>(buf.size())));
		if (internalWidth == narrow<unsigned>(buf.size())) {
			return std::span{internalData, buf.size()};
		} else {
			// slow path, non-inlined
			// internalData might be equal to buf
			scaleLine(std::span{internalData, internalWidth}, buf);
			return buf;
		}
	}

	/** Abstract implementation of getLinePtr().
	  * Pixel type is unspecified (implementations that care about the
	  * exact type should get it via some other mechanism).
	  * @param line The line number for the requested line.
	  * @param lineWidth Output parameter, the width of the returned line
	  *                  in pixel units.
	  * @param buf Buffer space that can _optionally_ be used by the
	  *            implementation.
	  * @param bufWidth The size of the above buffer, in pixel units.
	  * @return Pointer to the first pixel of the requested line. This might
	  *         be the same as the given 'buf' parameter or it might be some
	  *         internal buffer.
	  */
	[[nodiscard]] virtual const void* getLineInfo(
		unsigned line, unsigned& lineWidth,
		void* buf, unsigned bufWidth) const = 0;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 320x240 pixels. The difference between this method and
	  * getLinePtr() is that this method also does vertical scaling.
	  * This is used for video recording.
	  */
	[[nodiscard]] std::span<const Pixel, 320> getLinePtr320_240(unsigned line, std::span<Pixel, 320> buf) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 640x480 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	[[nodiscard]] std::span<const Pixel, 640> getLinePtr640_480(unsigned line, std::span<Pixel, 640> buf) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 960x720 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	[[nodiscard]] std::span<const Pixel, 960> getLinePtr960_720(unsigned line, std::span<Pixel, 960> buf) const;

	[[nodiscard]] const PixelFormat& getPixelFormat() const {
		return pixelFormat;
	}

protected:
	explicit FrameSource(const PixelFormat& format);
	~FrameSource() = default;

	void setHeight(unsigned height_) { height = height_; }

	/** Returns true when two consecutive rows are also consecutive in
	  * memory.
	  */
	[[nodiscard]] virtual bool hasContiguousStorage() const {
		return false;
	}

	void scaleLine(std::span<const Pixel> in, std::span<Pixel> out) const;

private:
	/** Pixel format. Needed for getLinePtr scaling
	  */
	const PixelFormat& pixelFormat;

	/** Number of lines in this frame.
	  */
	unsigned height;

	FieldType fieldType;
};

} // namespace openmsx

#endif // FRAMESOURCE_HH
