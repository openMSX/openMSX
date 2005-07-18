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
	/** Constructor.
	  */
	RawFrame(SDL_PixelFormat* format, bool oddField);

	/** Destructor.
	  */
	~RawFrame();

	/** Call this before using the object.
	  * It is also possible to reuse a RawFrame by calling this method.
	  * Reusing objects instead of reallocating them is important because we use
	  * 50 or 60 of them each second and the amount of data is rather large.
	  * @param oddField False for even fields, true for odd fields.
	  */
	void reinit(bool oddField);

	/** Remembers the type of pixels on a line.
	  * This is used to select the correct scaler algorithm for a line.
	  */
	enum LineContent {
		/** Line contains border colour.
		  */
		LINE_BLANK,
		/** Line contains 256 (wide) pixels.
		  */
		LINE_256,
		/** Line contains 512 (narrow) pixels.
		  */
		LINE_512,
	};

	inline SDL_Surface* getSurface() {
		return surface;
	}

	template <class Pixel>
	inline Pixel* getPixelPtr(int x, int y) {
	//inline Pixel* getPixelPtr(int x, int y, Pixel* dummy = 0) {
		return reinterpret_cast<Pixel*>(
			reinterpret_cast<byte*>(surface->pixels) + y * surface->pitch
			) + x;
	}

	inline LineContent getLineContent(unsigned line) {
		assert(0 <= line && line < HEIGHT);
		return lineContent[line];
	}

	// TODO: Replace this by "setBlank", "set256" and "set512"?
	inline void setLineContent(unsigned line, LineContent content) {
		assert(0 <= line && line < HEIGHT);
		lineContent[line] = content;
	}

	template <class Pixel>
	inline Pixel* setBlank(unsigned line, Pixel colour0, Pixel colour1) {
		assert(line < HEIGHT);
		Pixel* pixels = getPixelPtr<Pixel>(0, line);
		pixels[0] = colour0;
		pixels[1] = colour1; // TODO: We store colour1, but no-one uses it.
		lineContent[line] = LINE_BLANK;
	}

	inline bool isOddField() {
		return oddField;
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

	LineContent lineContent[HEIGHT];

	SDL_Surface* surface;

	bool oddField;

};

// Workaround for bug in GCC versions prior to 3.4.
//   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=795
// Note that we never define this function, just declaring it is enough to
// make GCC do the right thing.
template <class Pixel> void getPixelPtr();

} // namespace openmsx

#endif // RAWFRAME_HH
