#include "TTFFont.hh"

#include "LocalFileReference.hh"
#include "MSXException.hh"

#include "StringOp.hh"
#include "stl.hh"
#include "zstring_view.hh"

#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace openmsx {

class SDLTTF
{
public:
	SDLTTF(const SDLTTF&) = delete;
	SDLTTF(SDLTTF&&) = delete;
	SDLTTF& operator=(const SDLTTF&) = delete;
	SDLTTF& operator=(SDLTTF&&) = delete;

	static SDLTTF& instance();

private:
	SDLTTF();
	~SDLTTF();
};

class TTFFontPool
{
public:
	TTFFontPool(const TTFFontPool&) = delete;
	TTFFontPool(TTFFontPool&&) = delete;
	TTFFontPool& operator=(const TTFFontPool&) = delete;
	TTFFontPool& operator=(TTFFontPool&&) = delete;

	static TTFFontPool& instance();
	TTF_Font* get(const std::string& filename, int ptSize, int faceIndex);
	void release(TTF_Font* font);

private:
	TTFFontPool() = default;
	~TTFFontPool();

	// We want to keep the LocalFileReference object alive for as long as
	// the SDL_ttf library uses the font. This solves a problem we had in
	// the past on windows that temporary files were not cleaned up. The
	// scenario went like this:
	//  1. new LocalFileReference object (possibly) creates a temp file
	//  2. SDL_ttf opens this file (and keeps it open)
	//  3. LocalFileReference object goes out of scope and deletes the
	//     temp file
	//  4. (much later) we're done using the font and SDL_ttf closes the
	//     file
	// Step 3 goes wrong in windows because it's not possible to delete a
	// still opened file (no problem in unix). Solved by swapping the order
	// of step 3 and 4. Though this has the disadvantage that if openMSX
	// crashes between step 3 and 4 the temp file is still left behind.
	struct FontInfo {
		LocalFileReference file;
		TTF_Font* font;
		std::string name;
		int size;
		int count;
		int faceIndex;
	};
	std::vector<FontInfo> pool;
};


// class SDLTTF

SDLTTF::SDLTTF()
{
	if (!TTF_Init()) {
		throw FatalError("Couldn't initialize SDL_ttf: ", SDL_GetError());
	}
}

SDLTTF::~SDLTTF()
{
	TTF_Quit();
}

SDLTTF& SDLTTF::instance()
{
	static SDLTTF oneInstance;
	return oneInstance;
}


// class TTFFontPool

TTFFontPool::~TTFFontPool()
{
	assert(pool.empty());
}

TTFFontPool& TTFFontPool::instance()
{
	static TTFFontPool oneInstance;
	return oneInstance;
}

TTF_Font* TTFFontPool::get(const std::string& filename, int ptSize, int faceIndex)
{
	if (auto it = std::ranges::find(pool, std::tuple(filename, ptSize, faceIndex),
	        [](auto& info) { return std::tuple(info.name, info.size, info.faceIndex); });
	    it != end(pool)) {
		++it->count;
		return it->font;
	}

	SDLTTF::instance(); // init library
	FontInfo info;
	info.file = LocalFileReference(filename);

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, TTF_PROP_FONT_CREATE_FILENAME_STRING,
	                      info.file.getFilename().c_str());
	SDL_SetFloatProperty(props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, float(ptSize));
	SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, faceIndex);
	auto* result = TTF_OpenFontWithProperties(props);
	SDL_DestroyProperties(props);

	if (!result) {
		throw MSXException(SDL_GetError());
	}
	info.font = result;
	info.name = filename;
	info.size = ptSize;
	info.count = 1;
	info.faceIndex = faceIndex;
	pool.push_back(std::move(info));
	return result;
}

void TTFFontPool::release(TTF_Font* font)
{
	auto it = rfind_unguarded(pool, font, &FontInfo::font);
	--it->count;
	if (it->count == 0) {
		TTF_CloseFont(it->font);
		move_pop_back(pool, it);
	}
}


// class TTFFont

TTFFont::TTFFont(const std::string& filename, int ptSize, int faceIndex)
	: font(TTFFontPool::instance().get(filename, ptSize, faceIndex))
{
}

TTFFont::~TTFFont()
{
	if (!font) return;
	TTFFontPool::instance().release(static_cast<TTF_Font*>(font));
}

SDLSurfacePtr TTFFont::render(std::string text, uint8_t r, uint8_t g, uint8_t b) const
{
	SDL_Color color = { r, g, b, 0 };

	// Optimization: remove trailing empty lines
	StringOp::trimRight(text, " \n");
	if (text.empty()) return SDLSurfacePtr(nullptr);

	// Split on newlines
	auto lines_view = StringOp::split_view(text, '\n');
	auto lines_it = lines_view.begin();
	auto lines_end = lines_view.end();
	assert(lines_it != lines_end);

	auto current_line = *lines_it;
	++lines_it;
	if (lines_it == lines_end) {
		// Special case for a single line: we can avoid the
		// copy to an extra SDL_Surface
		assert(!text.empty());
		SDLSurfacePtr surface(
			TTF_RenderText_Blended(static_cast<TTF_Font*>(font),
			                       text.c_str(), 0, color));
		if (!surface) {
			throw MSXException(SDL_GetError());
		}
		return surface;
	}

	// Determine maximum width and lineHeight
	int width = 0;
	int lineHeight = 0; // initialize to avoid warning
	int numLines = 1;
	while (true) {
		auto [w, h] = getSize(std::string(current_line));
		width = std::max(width, w);
		lineHeight = h;
		if (lines_it == lines_end) break;
		current_line = *lines_it;
		++lines_it;
		++numLines;
	}
	// There might be extra space between two successive lines
	// (so lineSkip might be bigger than lineHeight).
	int lineSkip = getHeight();
	// We assume that height is the same for all lines.
	// For the last line we don't include spacing between two lines.
	auto height = (numLines - 1) * lineSkip + lineHeight;

	// Create destination surface (initial surface is fully transparent)
	SDLSurfacePtr destination(SDL_CreateSurface(width, height,
		    SDL_GetPixelFormatForMasks(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)));
	if (!destination) {
		throw MSXException("Couldn't allocate surface for multiline text.");
	}

	// Actually render the text:
	// TODO use enumerate() in the future (c++20)
	int i = -1;
	for (auto line : lines_view) {
		++i;
		// Render single line
		if (line.empty()) {
			// SDL_TTF gives an error on empty lines, but we can
			// simply skip such lines
			continue;
		}
		SDLSurfacePtr surf(TTF_RenderText_Blended(
			static_cast<TTF_Font*>(font),
			std::string(line).c_str(), 0, color));
		if (!surf) {
			throw MSXException(SDL_GetError());
		}

		// Copy line to destination surface
		SDL_Rect rect;
		rect.x = 0;
		rect.y = Sint16(i * lineSkip);
		SDL_SetSurfaceBlendMode(surf.get(), SDL_BLENDMODE_NONE); // no blending during copy
		SDL_BlitSurface(surf.get(), nullptr, destination.get(), &rect);
	}
	return destination;
}

int TTFFont::getHeight() const
{
	return TTF_GetFontLineSkip(static_cast<TTF_Font*>(font));
}

bool TTFFont::isFixedWidth() const
{
	return TTF_FontIsFixedWidth(static_cast<TTF_Font*>(font));
}

int TTFFont::getWidth() const
{
	int advance;
	if (TTF_GetGlyphMetrics(static_cast<TTF_Font*>(font), Uint16('M'),
	                        nullptr /*minx*/, nullptr /*maxx*/,
	                        nullptr /*miny*/, nullptr /*maxy*/,
	                        &advance)) {
		// error?
		return 10; //fallback-width
	}
	return advance;
}

gl::ivec2 TTFFont::getSize(zstring_view text) const
{
	int width, height;
	if (TTF_GetStringSize(static_cast<TTF_Font*>(font), text.c_str(), 0,
	                      &width, &height)) {
		throw MSXException(SDL_GetError());
	}
	return {width, height};
}

} // namespace openmsx
