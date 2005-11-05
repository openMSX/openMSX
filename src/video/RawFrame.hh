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
	RawFrame(unsigned bytesPerPixel, unsigned maxWidth, unsigned height);
	~RawFrame();

	virtual FieldType getField();
	virtual unsigned getLineWidth(unsigned line);

	/** Call this before using the object.
	  * It is also possible to reuse a RawFrame by calling this method.
	  * Reusing objects instead of reallocating them is important because we use
	  * 50 or 60 of them each second and the amount of data is rather large.
	  * @param field Interlacing status of this frame.
	  */
	void init(FieldType field);

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < height);
		assert(width <= maxWidth);
		lineWidth[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel color) {
		setBlank(line, color, color);
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel colour0, Pixel colour1) {
		assert(line < height);
		Pixel* pixels = getLinePtr(line, (Pixel*)0);
		pixels[0] = colour0;
		pixels[1] = colour1; // TODO: We store colour1, but no-one uses it.
		lineWidth[line] = 0;
	}

protected:
	virtual void* getLinePtrImpl(unsigned line);

private:
	unsigned maxWidth;
	unsigned* lineWidth;
	unsigned pitch;
	void* unallignedData;
	char* data;
	FieldType field;
};

} // namespace openmsx

#endif // RAWFRAME_HH
