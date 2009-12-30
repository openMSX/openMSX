// $Id$

#include "TTFFont.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
#include "shared_ptr.hh"
#include "unreachable.hh"
#include <SDL_ttf.h>
#include <map>
#include <algorithm>
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
		shared_ptr<LocalFileReference> file; // c++0x unique_ptr may also work
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
		throw FatalError(string("Couldn't initialize SDL_ttf: ") +
		                 TTF_GetError());
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
	info.file.reset(new LocalFileReference(filename));
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

SDLSurfacePtr TTFFont::render(const string& text, byte r, byte g, byte b)
{
	SDL_Color color = { r, g, b, 0 };
	SDLSurfacePtr surface(TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font),
	                                             text.c_str(), color));
	if (!surface.get()) {
		throw MSXException(TTF_GetError());
	}
	return surface;

	// TODO for GP2X copy to a HW_Surface?
}

unsigned TTFFont::getHeight()
{
	return TTF_FontLineSkip(static_cast<TTF_Font*>(font));
}

unsigned TTFFont::getWidth()
{
	int advance;
	if (TTF_GlyphMetrics(static_cast<TTF_Font*>(font), Uint16('M'),
	                     NULL /*minx*/, NULL /*maxx*/,
	                     NULL /*miny*/, NULL /*maxy*/,
	                     &advance)) {
		// error?
		return 10; //fallback-width
	}
	return advance;
}

} // namespace openmsx
