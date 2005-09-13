// $Id$

#ifndef FRAMESOURCE_HH
#define FRAMESOURCE_HH


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
		FIELD_ODD,
	};

	virtual ~FrameSource() {};

	/** Gets the role this frame plays in interlacing.
	  */
	virtual FieldType getField() = 0;

	/** Gets the number of display pixels on the given line.
	  * @return line width, or 0 for a border line.
	  */
	virtual unsigned getLineWidth(unsigned line) = 0;

	/** Gets a pointer to the pixels of the given line number.
	  * The dummy parameter is a workaround for bug in GCC versions
	  * prior to 3.4, see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=795
	  */
	template <class Pixel>
	inline Pixel* getLinePtr(unsigned line, Pixel* /*dummy*/) {
		return reinterpret_cast<Pixel*>(getLinePtrImpl(line));
	}

protected:
	/** Actual implementation of getLinePtr(unsigned, Pixel) but without
	  * a typed return value.
	  */
	virtual void* getLinePtrImpl(unsigned line) = 0;

};

} // namespace openmsx

#endif // FRAMESOURCE_HH
