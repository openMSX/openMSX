// $Id$

#ifndef TTFFONT_HH
#define TTFFONT_HH

#include "SDLSurfacePtr.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class TTFFont
{
public:
	/** Construct new TTFFont object.
	  * @param font Filename of font (.fft file, possibly (g)zipped).
	  * @param ptSize Point size (based on 72DPI) to load font as.
	  */
	TTFFont(const std::string& font, int ptSize);
	~TTFFont();

	/** Render the given text to a new SDL_Surface.
	  * The text must be UTF-8 encoded.
	  * The result is a 32bpp RGBA SDL_Surface.
	  */
	SDLSurfacePtr render(std::string text, byte r, byte g, byte b) const;

	/** Return the height of the font.
	  * This is the recommended number of pixels between two text lines.
	  */
	unsigned getHeight() const;

	/** Return the width of the font.
	  * This is the recommended number of pixels between two characters.
	  * This number only makes sense for fixed-width fonts.
	  */
	unsigned getWidth() const;

	/** Return the size in pixels of the text if it would be rendered.
	 */
	void getSize(const std::string& text, unsigned& width, unsigned& height) const;

private:
	void* font;  // TTF_Font*
};

} // namespace openmsx

#endif
