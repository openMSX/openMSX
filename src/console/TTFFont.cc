// $Id$

#include "TTFFont.hh"
#include "LocalFileReference.hh"
#include "MSXException.hh"
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

	typedef std::map<std::pair<string, int>, std::pair<TTF_Font*, int> > Pool;
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
		++(it->second.second);
		return it->second.first;
	}

	SDLTTF::instance(); // init library
	LocalFileReference file(filename);
	TTF_Font* font = TTF_OpenFont(file.getFilename().c_str(), ptSize);
	if (!font) {
		throw MSXException(TTF_GetError());
	}
	pool.insert(std::make_pair(key, std::make_pair(font, 1)));
	return font;
}

void TTFFontPool::release(TTF_Font* font)
{
	for (Pool::iterator it = pool.begin(); it != pool.end(); ++it) {
		if (it->second.first == font) {
			--(it->second.second);
			if (it->second.second == 0) {
				TTF_CloseFont(it->second.first);
				pool.erase(it);
			}
			return;
		}
	}
	assert(false);
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

SDL_Surface* TTFFont::render(const string& text, byte r, byte g, byte b)
{
	SDL_Color color = { r, g, b, 0 };
	SDL_Surface* surface = TTF_RenderUTF8_Blended(static_cast<TTF_Font*>(font),
	                                              text.c_str(), color);
	if (!surface) {
		throw MSXException(TTF_GetError());
	}
	return surface;

	// TODO for GP2X copy to a HW_Surface?
}

unsigned TTFFont::getFontHeight()
{
	return TTF_FontLineSkip(static_cast<TTF_Font*>(font));
}

unsigned TTFFont::getFontWidth()
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
