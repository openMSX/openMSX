// $Id$

#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH

#include "noncopyable.hh"
#include <algorithm>
#include <vector>
#include <cassert>

struct SDL_PixelFormat;

namespace openmsx {

/** Interface for getting lines from a video frame.
  */
class FrameSource : private noncopyable
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

	virtual ~FrameSource();

	/** (Re)initialize an existing FrameSource. This method sets the
	  * Fieldtype and flushes the 'getLinePtr' buffers.
	  */
	void init(FieldType fieldType);

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
	  * @return line width, or 0 for a border line.
	  */
	unsigned getLineWidth(unsigned line) const {
		unsigned width;
		getLineInfo(line, width); // ignore return value
		return width;
	}

	/** Get the width of (all) lines in this frame.
	 * This only makes sense when all lines have the same width, so this
	 * methods asserts that all lines actually have the same width. This
	 * is for example not always the case for MSX frames, but it is for
	 * video frames (for superimpose).
	 */
	unsigned getWidth() const {
		unsigned height = getHeight();
		assert(height > 0);
		unsigned result = getLineWidth(0);
		for (unsigned line = 1; line < height; ++line) {
			assert(result == getLineWidth(line));
		}
		return result;
	}

	/** Gets a pointer to the pixels of the given line number.
	  */
	template <typename Pixel>
	inline const Pixel* getLinePtr(unsigned line) const {
		unsigned dummy;
		return reinterpret_cast<const Pixel*>(getLineInfo(line, dummy));
	}

	/** Gets a pointer to the pixels of the given line number.
	  * The line returned is guaranteed to have the given width; if the
	  * original line had a different width it will be scaled in a temporary
	  * buffer. You should call freeLineBuffers regularly so the allocated
	  * buffers can be recycled.
	  */
	template <typename Pixel>
	inline const Pixel* getLinePtr(int line, unsigned width) const
	{
		line = std::min<unsigned>(std::max(0, line), getHeight() - 1);
		unsigned internalWidth;
		const Pixel* internalData = reinterpret_cast<const Pixel*>(
			getLineInfo(line, internalWidth));
		if (internalWidth == width) {
			return internalData;
		} else {
			// slow path, non-inlined
			return scaleLine(internalData, internalWidth, width);
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
		unsigned width) const
	{
		actualLines = 1;
		int height = getHeight();
		if ((line < 0) || (height <= line)) {
			return getLinePtr<Pixel>(line, width);
		}
		unsigned internalWidth;
		const Pixel* internalData = reinterpret_cast<const Pixel*>(
			getLineInfo(line, internalWidth));
		if (internalWidth != width) {
			return scaleLine(internalData, internalWidth, width);
		}
		if (!hasContiguousStorage()) {
			return internalData;
		}
		while (--numLines) {
			++line;
			if ((line == height) || (getLineWidth(line) != width)) {
				break;
			}
			++actualLines;
		}
		return internalData;
	}

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 320x240 pixels. The difference between this method and
	  * getLinePtr() is that this method also does vertical scaling.
	  * You should also call freeLineBuffers() to release possible internal
	  * allocated buffers.
	  * This is used for video recording.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr320_240(unsigned line) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 640x480 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr640_480(unsigned line) const;

	/** Get a pointer to a given line in this frame, the frame is scaled
	  * to 960x720 pixels. Same as getLinePtr320_240, but then for a
	  * higher resolution output.
	  */
	template <typename Pixel>
	const Pixel* getLinePtr960_720(unsigned line) const;

	/** Returns the distance (in pixels) between two consecutive lines.
	  * Is meant to be used in combination with getMultiLinePtr(). The
	  * result is only meaningful when hasContiguousStorage() returns
	  * true (also only in that case does getMultiLinePtr() return more
	  * than 1 line).
	  */
	virtual unsigned getRowLength() const {
		return 0;
	}

	/** Recycles the buffers allocated for scaling lines, see getLinePtr.
	  */
	void freeLineBuffers() const;

	const SDL_PixelFormat& getSDLPixelFormat() const {
		return pixelFormat;
	}

	// Used by SuperImposedFrame.
	// TODO refactor
	void* getTempBuffer() const;

protected:
	explicit FrameSource(const SDL_PixelFormat& format);

	void setHeight(unsigned height);

	/** Actual implementation of getLinePtr(unsigned, Pixel) but without
	  * a typed return value.
	  * TODO
	  */
	virtual const void* getLineInfo(unsigned line, unsigned& width) const = 0;

	/** Returns true when two consecutive rows are also consecutive in
	  * memory.
	  */
	virtual bool hasContiguousStorage() const {
		return false;
	}

	// TODO: I don't understand why I need to declare a subclass as friend
	//       to give it access to a protected method, but without this
	//       GCC 3.3.5-pre will not compile it.
	friend class DeinterlacedFrame;
	friend class DoubledFrame;

private:
	template <typename Pixel> const Pixel* scaleLine(
		const Pixel* in, unsigned inWidth, unsigned outWidth) const;
	template <typename Pixel> const Pixel* blendLines(
		const Pixel* line1, const Pixel* line2, unsigned width) const;

	/** Pixel format. Needed for getLinePtr scaling
	  */
	const SDL_PixelFormat& pixelFormat;

	/** Number of lines in this frame.
	  */
	unsigned height;

	FieldType fieldType;
	mutable std::vector<void*> tempBuffers;
	mutable unsigned tempCounter;
};

} // namespace openmsx

#endif // FRAMESOURCE_HH
