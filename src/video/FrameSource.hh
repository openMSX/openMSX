// $Id$

#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH

#include "noncopyable.hh"
#include <algorithm>
#include <vector>

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

	/** Returns the buffer size (in bytes) needed to store a line (of
	  * maximum width) of this frame.
	  */
	virtual unsigned getLineBufferSize() const = 0;

	/** Gets the number of display pixels on the given line.
	  * @return line width, or 0 for a border line.
	  */
	virtual unsigned getLineWidth(unsigned line) = 0;

	/** Gets a pointer to the pixels of the given line number.
	  * The dummy parameter is a workaround for bug in GCC versions
	  * prior to 3.4, see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=795
	  */
	template <typename Pixel>
	inline Pixel* getLinePtr(unsigned line, Pixel* /*dummy*/) {
		return reinterpret_cast<Pixel*>(getLinePtrImpl(line));
	}

	/** Gets a pointer to the pixels of the given line number.
	  * The line returned is guaranteed to have the given width; if the
	  * original line had a different width it will be scaled in a temporary
	  * buffer. You should call freeLineBuffers regularly so the allocated
	  * buffers can be recycled.
	  * The dummy parameter is a workaround for bug in GCC versions
	  * prior to 3.4, see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=795
	  */
	template <typename Pixel>
	inline const Pixel* getLinePtr(int line, unsigned width, Pixel* dummy) {
		line = std::min<unsigned>(std::max(0, line), getHeight() - 1);
		const Pixel* internalData = getLinePtr(line, dummy);
		unsigned internalWidth = getLineWidth(line);
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
		unsigned width, Pixel* dummy)
	{
		actualLines = 1;
		int height = getHeight();
		if ((line < 0) || (height <= line)) {
			return getLinePtr(line, width, dummy);
		}
		const Pixel* internalData = getLinePtr(line, dummy);
		unsigned internalWidth = getLineWidth(line);
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

	/** TODO used for avi recording */
	template <typename Pixel>
	const Pixel* getLinePtr320_240(unsigned line, Pixel* dummy);

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
	void freeLineBuffers();

protected:
	explicit FrameSource(const SDL_PixelFormat* format);

	void setHeight(unsigned height);

	/** Actual implementation of getLinePtr(unsigned, Pixel) but without
	  * a typed return value.
	  */
	virtual void* getLinePtrImpl(unsigned line) = 0;

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
	template <typename Pixel>
	const Pixel* scaleLine(const Pixel* in, unsigned inWidth, unsigned outWidth);

	void* getTempBuffer();

	/** Pixel format. Needed for getLinePtr scaling
	  */
	const SDL_PixelFormat* pixelFormat;

	/** Number of lines in this frame.
	  */
	unsigned height;

	FieldType fieldType;
	std::vector<void*> tempBuffers;
	unsigned tempCounter;
};

} // namespace openmsx

#endif // FRAMESOURCE_HH
