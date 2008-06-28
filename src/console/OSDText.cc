// $Id$

#include "OSDText.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "LocalFileReference.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include <SDL_ttf.h>
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif
#include <cassert>
#include <map>

using std::string;
using std::pair;
using std::map;
using std::make_pair;

namespace openmsx {

class SDLTTF
{
public:
	static SDLTTF& instance();

private:
	SDLTTF();
	~SDLTTF();
};

class TTFFont
{
public:
	TTFFont(const std::string& font, int ptSize);
	~TTFFont();
	SDL_Surface* render(const string& text, byte r, byte g, byte b);

private:
	TTF_Font* font;
};

class TTFFontPool
{
public:
	static TTFFontPool& instance();
	TTFFont* get(const string& filename, int ptSize);
	void release(TTFFont* font);

private:
	TTFFontPool();
	~TTFFontPool();

	typedef map<pair<string, int>, pair<TTFFont*, int> > Pool;
	Pool pool;
};


// class OSDText

OSDText::OSDText(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, fontfile("skins/Vera.ttf.gz")
	, size(12)
	, font(NULL)
{
}

OSDText::~OSDText()
{
	invalidateLocal(); // release font
}

void OSDText::getProperties(std::set<std::string>& result) const
{
	result.insert("-text");
	result.insert("-font");
	result.insert("-size");
	OSDImageBasedWidget::getProperties(result);
}

void OSDText::setProperty(const std::string& name, const std::string& value)
{
	if (name == "-text") {
		text = value;
		// note: don't invalidate font (don't reopen font file)
		OSDImageBasedWidget::invalidateLocal();
		invalidateChildren();
	} else if (name == "-font") {
		SystemFileContext context;
		CommandController* controller = NULL; // ok for SystemFileContext
		string file = context.resolve(*controller, value);
		if (!FileOperations::isRegularFile(file)) {
			throw CommandException("Not a valid font file: " + value);
		}
		fontfile = value;
		invalidateRecursive();
	} else if (name == "-size") {
		size = StringOp::stringToInt(value);
		invalidateRecursive();
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

std::string OSDText::getProperty(const std::string& name) const
{
	if (name == "-text") {
		return text;
	} else if (name == "-font") {
		return fontfile;
	} else if (name == "-size") {
		return StringOp::toString(size);
	} else {
		return OSDImageBasedWidget::getProperty(name);
	}
}

void OSDText::invalidateLocal()
{
	if (font) {
		TTFFontPool::instance().release(font);
		font = NULL;
	}
	OSDImageBasedWidget::invalidateLocal();
}


std::string OSDText::getType() const
{
	return "text";
}

template <typename IMAGE> BaseImage* OSDText::create(OutputSurface& output)
{
	if (!font) {
		try {
			SystemFileContext context;
			CommandController* controller = NULL; // ok for SystemFileContext
			string file = context.resolve(*controller, fontfile);
			int ptSize = size * getScaleFactor(output);
			font = TTFFontPool::instance().get(file, ptSize);
		} catch (MSXException& e) {
			throw MSXException("Couldn't open font: " + e.getMessage());
		}
	}
	try {
		SDL_Surface* surface = font->render(
			text, getRed(), getGreen(), getBlue());
		return new IMAGE(output, surface);
	} catch (MSXException& e) {
		throw MSXException("Couldn't render text: " + e.getMessage());
	}
}

BaseImage* OSDText::createSDL(OutputSurface& output)
{
	return create<SDLImage>(output);
}

BaseImage* OSDText::createGL(OutputSurface& output)
{
#ifdef COMPONENT_GL
	return create<GLImage>(output);
#endif
	(void)output;
	return NULL;
}


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


// class TTFFont

TTFFont::TTFFont(const std::string& filename, int ptSize)
{
	SDLTTF::instance(); // init library

	LocalFileReference file(filename);
	font = TTF_OpenFont(file.getFilename().c_str(), ptSize);
	if (!font) {
		throw MSXException(TTF_GetError());
	}
}

TTFFont::~TTFFont()
{
	TTF_CloseFont(font);
}

SDL_Surface* TTFFont::render(const string& text, byte r, byte g, byte b)
{
	SDL_Color color = { r, g, b, 0 };
	SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
	if (!surface) {
		throw MSXException(TTF_GetError());
	}
	return surface;

	// TODO for GP2X copy to a HW_Surface?
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

TTFFont* TTFFontPool::get(const string& filename, int ptSize)
{
	Pool::key_type key = make_pair(filename, ptSize);
	Pool::iterator it = pool.find(key);
	if (it != pool.end()) {
		++(it->second.second);
		return it->second.first;
	}
	TTFFont* font = new TTFFont(filename, ptSize);
	pool.insert(make_pair(key, make_pair(font, 1)));
	return font;
}

void TTFFontPool::release(TTFFont* font)
{
	for (Pool::iterator it = pool.begin(); it != pool.end(); ++it) {
		if (it->second.first == font) {
			--(it->second.second);
			if (it->second.second == 0) {
				delete it->second.first;
				pool.erase(it);
			}
			return;
		}
	}
	assert(false);
}

} // namespace openmsx
