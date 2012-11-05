// $Id$

#include "TTFFont.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include <SDL_ttf.h>
#include <map>
#include <algorithm>
#include <memory>
#include <vector>
#include <iostream>
#include <cassert>

using std::string;
using std::vector;

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
		std::shared_ptr<LocalFileReference> file; // TODO use c++11 unique_ptr
		TTF_Font* font;
		int count;
	};
	typedef std::map<std::pair<string, int>, FontInfo> Pool;
	Pool pool;
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
	Pool::key_type key = make_pair(filename, ptSize);
	Pool::iterator it = pool.find(key);
	if (it != pool.end()) {
		++(it->second.count);
		return it->second.font;
	}

	SDLTTF::instance(); // init library
	FontInfo info;
	info.file = std::make_shared<LocalFileReference>(filename);
	info.font = TTF_OpenFont(info.file->getFilename().c_str(), ptSize);
	if (!info.font) {
		throw MSXException(TTF_GetError());
	}
	info.count = 1;
	pool.insert(std::make_pair(key, info));
	return info.font;
}

void TTFFontPool::release(TTF_Font* font)
{
	for (Pool::iterator it = pool.begin(); it != pool.end(); ++it) {
		if (it->second.font == font) {
			--(it->second.count);
			if (it->second.count == 0) {
				TTF_CloseFont(it->second.font);
				pool.erase(it);
			}
			return;
		}
	}
	UNREACHABLE;
}


// class TTFFont

TTFFont::TTFFont(const std::string& filename, int ptSize)
{
	font = TTFFontPool::instance().get(filename, ptSize);
}

TTFFont::~TTFFont()
{
	TTFFontPool::instance().release(static_cast<TTF_Font*>(font));
}

SDLSurfacePtr TTFFont::render(std::string text, byte r, byte g, byte b) const
{
	SDL_Color color = { r, g, b, 0 };

	// Optimization: remove trailing empty lines
	StringOp::trimRight(text, " \n");
	if (text.empty()) return SDLSurfacePtr(nullptr);

	// Split on newlines
	std::vector<string> lines;
	StringOp::split(text, "\n", lines);
	assert(!lines.empty());

	if (lines.size() == 1) {
		// Special case for a single line: we can avoid the
		// copy to an extra SDL_Surface
		assert(!text.empty());
		SDLSurfacePtr surface(
			TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font),
			                       text.c_str(), color));
		if (!surface.get()) {
			throw MSXException(TTF_GetError());
		}
		return surface;
	}

	// Determine maximum width and lineHeight
	unsigned width = 0;
	unsigned lineHeight = 0; // initialize to avoid warning
	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it) {
		unsigned w;
		getSize(*it, w, lineHeight);
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
	if (!destination.get()) {
		throw MSXException("Couldn't allocate surface for multiline text.");
	}

	// Actually render the text:
	for (unsigned i = 0; i < lines.size(); ++i) {
		// Render single line
		if (lines[i].empty()) {
			// SDL_TTF gives an error on empty lines, but we can
			// simply skip such lines
			continue;
		}
		SDLSurfacePtr line(TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font),
		                                          lines[i].c_str(), color));
		if (!line.get()) {
			throw MSXException(TTF_GetError());
		}

		// Copy line to destination surface
		SDL_Rect rect;
		rect.x = 0;
		rect.y = i * lineSkip;
		SDL_SetAlpha(line.get(), 0, 0); // no blending during copy
		SDL_BlitSurface(line.get(), nullptr, destination.get(), &rect);
	}
	return destination;

	// TODO for GP2X copy to a HW_Surface?
}

unsigned TTFFont::getHeight() const
{
	return TTF_FontLineSkip(static_cast<TTF_Font*>(font));
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
