#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH

#include "aligned.hh"
#include <algorithm>
#include <cassert>

struct SDL_PixelFormat;

namespace openmsx {

/** Interface for getting lines from a video frame.
  */
class FrameSource
{
public:
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
	FieldType getField() const {
		return fieldType;
	}

	/** Gets the number of lines in this frame.
	  */
	unsigned getHeight() const {
		return height;
	}

	/** Gets the number of display pixels on the given line.
	  * @return line width (=1 for a vertical border line)
	  */
	virtual unsigned getLineWidth(unsigned line) const = 0;

	/** Get the width of (all) lines in this frame.
	 * This only makes sense when all lines have the same width, so this
	 * methods asserts that all lines actually have the same width. This
	 * is for example not always the case for MSX frames, but it is for
	 * video frames (for superimpose).
	 */
	unsigned getWidth() const {
		assert(height > 0);
		unsigned result = getLineWidth(0);
		for (unsigned line = 1; line < height; ++line) {
			assert(result == getLineWidth(line));
		}
		return result;
	}

	/** Get the (single) color of the given line.
	  * Typically this will be used to get the color of a vertical border
	  * line. But it's fine to call this on non-border lines as well, in
	  * that case the color of the first pixel of the line is returned.
	  */
	template <typename Pixel>
	inline const Pixel getLineColor(unsigned line) const {
		SSE_ALIGNED(Pixel buf[1280]); // large enough for widest line
		unsigned width; // not used
		return reinterpret_cast<const Pixel*>(
			getLineInfo(line, width, buf, 1280))[0];
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
	template <typename Pixel>
	inline const Pixel* getLinePtr(int line, unsigned width, Pixel* buf) const
	{
		line = std::min<unsigned>(std::max(0, line), getHeight() - 1);
		unsigned internalWidth;
		auto* internalData = reinterpret_cast<const Pixel*>(
			getLineInfo(line, internalWidth, buf, width));
		if (internalWidth == width) {
			return internalData;
		} else {
			// slow path, non-inlined
			// internalData might be equal to buf
			scaleLine(internalData, buf, internalWidth, width);
			return buf;
		}
	}

	/** Similar to the above getLinePtr() method, but now tries to get
	  * multiple lines at once. This is not always possible, so the actual
	  * number of lines is returned in 'actualLines', it will always be at
	  * least 1.
	  */
	template <typename Pixel>
	inline const Pixel* getMultiLinePtr(
		int line, unsigned numLines, unsigned& actualLines,
		unsigned width, Pixel* buf) const
	{
		actualLines = 1;
		if ((line < 0) || (int(height) <= line)) {
			return getLinePtr(line, width, buf);
		}
		unsigned internalWidth;
		auto* internalData = reinterpret_cast<const Pixel*>(
			getLineInfo(line, internalWidth, buf, width));
		if (internalWidth != width) {
			scaleLine(internalData, buf, internalWidth, width);
			return buf;
		}
		if (!hasContiguousStorage()) {
			return internalData;
		}
		while (--numLines) {
			++line;
			if ((line == int(height)) || (getLineWidth(line) != width)) {
				break;
			}
			++actualLines;
		}
		return internalData;
	}

	/** Abstract implementation of getLinePtr().
	  * Pixel type is unspecified (implementations that care about the
	  * exact type should get it via some other mechanism).
	  * @param line The line number for the requisted line.
	  * @param lineWidth Output parameter, the width of the returned line
	  *                  in pixel units.
	  * @param buf Buffer space that can _optionally_ be used by the
	  *            implementation.
	  * @param bufWidth The size of the above buffer, in pixel units.
	  * @return Pointer to the first pixel of the requested line. This might
	  *         be the same as the given 'buf' parameter or it might be some
	  *         internal buffer.
	  */
	virtual const void* getLineInfo(
		unsigned line, unsigned& lineWidth,
		void* buf, unsigned bufWidth) const = 0;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 320x240 pixels. The difference between this method and
	  * getLinePtr() is that this method also does vertical scaling.
	  * This is used for video recording.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr320_240(unsigned line, Pixel* buf) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 640x480 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr640_480(unsigned line, Pixel* buf) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 960x720 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr960_720(unsigned line, Pixel* buf) const;

	/** Returns the distance (in pixels) between two consecutive lines.
	  * Is meant to be used in combination with getMultiLinePtr(). The
	  * result is only meaningful when hasContiguousStorage() returns
	  * true (also only in that case does getMultiLinePtr() return more
	  * than 1 line).
	  */
	virtual unsigned getRowLength() const {
		return 0;
	}

	const SDL_PixelFormat& getSDLPixelFormat() const {
		return pixelFormat;
	}

protected:
	explicit FrameSource(const SDL_PixelFormat& format);
	~FrameSource() {}

	void setHeight(unsigned height_) { height = height_; }

	/** Returns true when two consecutive rows are also consecutive in
	  * memory.
	  */
	virtual bool hasContiguousStorage() const {
		return false;
	}

	template <typename Pixel> void scaleLine(
		const Pixel* in, Pixel* out,
		unsigned inWidth, unsigned outWidth) const;

private:
	/** Pixel format. Needed for getLinePtr scaling
	  */
	const SDL_PixelFormat& pixelFormat;

	/** Number of lines in this frame.
	  */
	unsigned height;

	FieldType fieldType;
};

} // namespace openmsx

#endif // FRAMESOURCE_HH
