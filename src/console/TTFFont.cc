#include "TTFFont.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "stl.hh"
#include "xrange.hh"
#include <SDL_ttf.h>
#include <algorithm>
#include <vector>
#include <cassert>

using std::string;

namespace openmsx {

class SDLTTF
{
public:
	static SDLTTF& instance();

private:
	SDLTTF();
	~SDLTTF();
};

class TTFFontPool
{
public:
	static TTFFontPool& instance();
	TTF_Font* get(const string& filename, int ptSize);
	void release(TTF_Font* font);

private:
	TTFFontPool();
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
	};
	std::vector<FontInfo> pool;
};


// class SDLTTF

SDLTTF::SDLTTF()
{
	if (TTF_Init() < 0) {
		throw FatalError(StringOp::Builder() <<
			"Couldn't initialize SDL_ttf: " << TTF_GetError());
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

TTFFontPool::TTFFontPool()
{
}

TTFFontPool::~TTFFontPool()
{
	assert(pool.empty());
}

TTFFontPool& TTFFontPool::instance()
{
	static TTFFontPool oneInstance;
	return oneInstance;
}

TTF_Font* TTFFontPool::get(const string& filename, int ptSize)
{
	auto it = find_if(begin(pool), end(pool),
		[&](const FontInfo& i) {
			return (i.name == filename) && (i.size == ptSize); });
	if (it != end(pool)) {
		++it->count;
		return it->font;
	}

	SDLTTF::instance(); // init library
	FontInfo info;
	info.file = LocalFileReference(filename);
	auto* result = TTF_OpenFont(info.file.getFilename().c_str(), ptSize);
	if (!result) {
		throw MSXException(TTF_GetError());
	}
	info.font = result;
	info.name = filename;
	info.size = ptSize;
	info.count = 1;
	pool.push_back(std::move(info));
	return result;
}

void TTFFontPool::release(TTF_Font* font)
{
	auto it = rfind_if_unguarded(pool,
		[&](const FontInfo& i) { return i.font == font; });
	--it->count;
	if (it->count == 0) {
		TTF_CloseFont(it->font);
		move_pop_back(pool, it);
	}
}


// class TTFFont

TTFFont::TTFFont(const std::string& filename, int ptSize)
{
	font = TTFFontPool::instance().get(filename, ptSize);
}

TTFFont::~TTFFont()
{
	if (!font) return;
	TTFFontPool::instance().release(static_cast<TTF_Font*>(font));
}

SDLSurfacePtr TTFFont::render(std::string text, byte r, byte g, byte b) const
{
	SDL_Color color = { r, g, b, 0 };

	// Optimization: remove trailing empty lines
	StringOp::trimRight(text, " \n");
	if (text.empty()) return SDLSurfacePtr(nullptr);

	// Split on newlines
	auto lines = StringOp::split(text, '\n');
	assert(!lines.empty());

	if (lines.size() == 1) {
		// Special case for a single line: we can avoid the
		// copy to an extra SDL_Surface
		assert(!text.empty());
		SDLSurfacePtr surface(
			TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font),
			                       text.c_str(), color));
		if (!surface) {
			throw MSXException(TTF_GetError());
		}
		return surface;
	}

	// Determine maximum width and lineHeight
	unsigned width = 0;
	unsigned lineHeight = 0; // initialize to avoid warning
	for (auto& s : lines) {
		unsigned w;
		getSize(s.str(), w, lineHeight);
		width = std::max(width, w);
	}
	// There might be extra space between two successive lines
	// (so lineSkip might be bigger than lineHeight).
	unsigned lineSkip = getHeight();
	// We assume that height is the same for all lines.
	// For the last line we don't include spacing between two lines.
	unsigned height = unsigned((lines.size() - 1) * lineSkip + lineHeight);

	// Create destination surface (initial surface is fully transparent)
	SDLSurfacePtr destination(SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
			32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
	if (!destination) {
		throw MSXException("Couldn't allocate surface for multiline text.");
	}

	// Actually render the text:
	for (auto i : xrange(lines.size())) {
		// Render single line
		if (lines[i].empty()) {
			// SDL_TTF gives an error on empty lines, but we can
			// simply skip such lines
			continue;
		}
		SDLSurfacePtr line(TTF_RenderUTF8_Blended(
			static_cast<TTF_Font*>(font),
			lines[i].str().c_str(), color));
		if (!line) {
			throw MSXException(TTF_GetError());
		}

		// Copy line to destination surface
		SDL_Rect rect;
		rect.x = 0;
		rect.y = Sint16(i * lineSkip);
		SDL_SetAlpha(line.get(), 0, 0); // no blending during copy
		SDL_BlitSurface(line.get(), nullptr, destination.get(), &rect);
	}
	return destination;
}

unsigned TTFFont::getHeight() const
{
	return TTF_FontLineSkip(static_cast<TTF_Font*>(font));
}

bool TTFFont::isFixedWidth() const
{
	return TTF_FontFaceIsFixedWidth(static_cast<TTF_Font*>(font)) != 0;
}

unsigned TTFFont::getWidth() const
{
	int advance;
	if (TTF_GlyphMetrics(static_cast<TTF_Font*>(font), Uint16('M'),
	                     nullptr /*minx*/, nullptr /*maxx*/,
	                     nullptr /*miny*/, nullptr /*maxy*/,
	                     &advance)) {
		// error?
		return 10; //fallback-width
	}
	return advance;
}

void TTFFont::getSize(const std::string& text,
                      unsigned& width, unsigned& height) const
{
	if (TTF_SizeUTF8(static_cast<TTF_Font*>(font), text.c_str(),
	                 reinterpret_cast<int*>(&width),
	                 reinterpret_cast<int*>(&height))) {
		throw MSXException(TTF_GetError());
	}
}

} // namespace openmsx
