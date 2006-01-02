// $Id$

#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH

#include <algorithm>
#include <vector>

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

protected:
	FrameSource(const SDL_PixelFormat* format);

	void setHeight(unsigned height);

	/** Actual implementation of getLinePtr(unsigned, Pixel) but without
	  * a typed return value.
	  */
	virtual void* getLinePtrImpl(unsigned line) = 0;
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
