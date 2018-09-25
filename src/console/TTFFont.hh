#ifndef TTFFONT_HH
#define TTFFONT_HH

#include "SDLSurfacePtr.hh"
#include "openmsx.hh"
#include <algorithm>
#include <string>

namespace openmsx {

class TTFFont
{
public:
	TTFFont(const TTFFont&) = delete;
	TTFFont& operator=(const TTFFont&) = delete;

	/** Construct an empty font.
	  * The only valid operations on empty font objects are:
	  *  - (move)-assign a different value to it
	  *  - destruct the object
	  * post-condition: empty()
	  */
	TTFFont() : font(nullptr) {}

	/** Construct new TTFFont object.
	  * @param filename Filename of font (.fft file, possibly (g)zipped).
	  * @param ptSize Point size (based on 72DPI) to load font as.
	  * post-condition: !empty()
	  */
	TTFFont(const std::string& filename, int ptSize);

	/** Move construct. */
	TTFFont(TTFFont&& other) noexcept
		: font(other.font)
	{
		other.font = nullptr;
	}

	/** Move assignment. */
	TTFFont& operator=(TTFFont&& other) noexcept
	{
		std::swap(font, other.font);
		return *this;
	}

	~TTFFont();

	/** Is this an empty font? (a default constructed object). */
	bool empty() const { return font == nullptr; }

	/** Render the given text to a new SDL_Surface.
	  * The text must be UTF-8 encoded.
	  * The result is a 32bpp RGBA SDL_Surface.
	  */
	SDLSurfacePtr render(std::string text, byte r, byte g, byte b) const;

	/** Return the height of the font.
	  * This is the recommended number of pixels between two text lines.
	  */
	unsigned getHeight() const;

	/** Returns true iff this is a fixed-with (=mono-spaced) font. */
	bool isFixedWidth() const;

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
