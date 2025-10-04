#ifndef TTFFONT_HH
#define TTFFONT_HH

#include "SDLSurfacePtr.hh"
#include "gl_vec.hh"
#include "zstring_view.hh"
#include <cstdint>
#include <string>
#include <utility>

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
	TTFFont() = default;

	/** Construct new TTFFont object.
	  * @param filename Filename of font (.ttf/.ttc/.otf, possibly (g)zipped).
	  * @param ptSize Point size (based on 72DPI) to load font as.
	  * @param faceIndex The face number when filename points .ttc font.
	  * post-condition: !empty()
	  */
	TTFFont(const std::string& filename, int ptSize, int faceIndex);

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
	[[nodiscard]] bool empty() const { return font == nullptr; }

	/** Render the given text to a new SDL_Surface.
	  * The text must be UTF-8 encoded.
	  * The result is a 32bpp RGBA SDL_Surface.
	  */
	[[nodiscard]] SDLSurfacePtr render(std::string text, uint8_t r, uint8_t g, uint8_t b) const;

	/** Return the height of the font.
	  * This is the recommended number of pixels between two text lines.
	  */
	[[nodiscard]] int getHeight() const;

	/** Returns true iff this is a fixed-with (=mono-spaced) font. */
	[[nodiscard]] bool isFixedWidth() const;

	/** Return the width of the font.
	  * This is the recommended number of pixels between two characters.
	  * This number only makes sense for fixed-width fonts.
	  */
	[[nodiscard]] int getWidth() const;

	/** Return the size in pixels of the text if it would be rendered.
	 */
	[[nodiscard]] gl::ivec2 getSize(zstring_view text) const;

private:
	void* font = nullptr;  // TTF_Font*
};

} // namespace openmsx

#endif
