// $Id$

#ifndef RAWFRAME_HH
#define RAWFRAME_HH

// For definition of "byte", remove later?
#include "openmsx.hh"

// Get rid of this later.
#include <SDL.h>

#include <cassert>


namespace openmsx {

/** A video frame as output by the VDP scanline conversion unit,
  * before any postprocessing filters are applied.
  */
class RawFrame
{
public:
	/** What role does this frame play in interlacing?
	  */
	enum FieldType {
		/** Interlacing is off for this frame */
		FIELD_NONINTERLACED,
		/** Interlacing is on and this is an even frame */
		FIELD_EVEN,
		/** Interlacing is on and this is an odd frame */
		FIELD_ODD,
	};

	RawFrame(SDL_PixelFormat* format, FieldType field);
	~RawFrame();

	/** Call this before using the object.
	  * It is also possible to reuse a RawFrame by calling this method.
	  * Reusing objects instead of reallocating them is important because we use
	  * 50 or 60 of them each second and the amount of data is rather large.
	  * @param field Interlacing status of this frame.
	  */
	void reinit(FieldType field);

	inline SDL_Surface* getSurface() {
		return surface;
	}

	template <class Pixel>
	//inline Pixel* getPixelPtr(int x, int y) {
	inline Pixel* getPixelPtr(int x, int y, Pixel* /*dummy*/ = 0) {
		return reinterpret_cast<Pixel*>(
			reinterpret_cast<byte*>(surface->pixels) + y * surface->pitch
			) + x;
	}

	inline unsigned getLineWidth(unsigned line) {
		assert(line < HEIGHT);
		return lineWidth[line];
	}

	inline void setLineWidth(unsigned line, unsigned width) {
		assert(line < HEIGHT);
		assert(width <= (unsigned)surface->w);
		lineWidth[line] = width;
	}

	template <class Pixel>
	inline void setBlank(unsigned line, Pixel colour0, Pixel colour1) {
		assert(line < HEIGHT);
		Pixel* pixels = getPixelPtr<Pixel>(0, line);
		pixels[0] = colour0;
		pixels[1] = colour1; // TODO: We store colour1, but no-one uses it.
		lineWidth[line] = 0;
	}

	inline FieldType getField() {
		return field;
	}

private:

	// TODO: Use MSX-derived dimensions instead?
	//       That would be better if we ever want to support resizable screens.

	/** Horizontal dimensions of the screen.
	  */
	static const unsigned WIDTH = 640;

	/** Vertical dimensions of the screen.
	  */
	static const unsigned HEIGHT = 240;

	unsigned lineWidth[HEIGHT];

	SDL_Surface* surface;

	FieldType field;
};

// Workaround for bug in GCC versions prior to 3.4.
//   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=795
// Note that we never define this function, just declaring it is enough to
// make GCC do the right thing.
//template <class Pixel> void getPixelPtr();

} // namespace openmsx

#endif // RAWFRAME_HH
