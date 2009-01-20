// $Id$

#ifndef TTFFONT_HH
#define TTFFONT_HH

#include "openmsx.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class TTFFont
{
public:
	/** Construct new TTFFont object.
	  * @param font Filename of font (.fft file, possibly (g)zipped).
	  * @param prSize Point size (based on 72DPI) to load font as.
	  */
	TTFFont(const std::string& font, int ptSize);
	~TTFFont();

	/** Render the given text to a new SDL_Surface.
	  * The text must be UTF-8 encoded.
	  * The result is a 32bpp RGBA SDL_Surface, this surface must be freed
	  * by the caller.
	  */
	SDL_Surface* render(const std::string& text, byte r, byte g, byte b);

	/** Return the height of the font.
	  * This is the recommended number of pixels between two text lines.
	  */
	unsigned getFontHeight();

	/** Return the width of the font.
	  * This is the recommended number of pixels between two characters.
	  * This number only makes sense for fixed-width fonts.
	  */
	unsigned getFontWidth();

private:
	void* font;  // TTF_Font*
};

} // namespace openmsx

#endif
