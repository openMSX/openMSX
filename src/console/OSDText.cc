// $Id$

#include "OSDText.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include <SDL_ttf.h>
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

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

class TTFFont
{
public:
	TTFFont(std::string& font, int ptsize);
	~TTFFont();
	SDL_Surface* render(const string& text, byte r, byte g, byte b);

private:
	TTF_Font* font;
};


// class OSDText

OSDText::OSDText(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, fontfile("notepad.ttf")
	, size(12)
{
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
		if (!FileOperations::isRegularFile(value)) {
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
	font.reset();
	OSDImageBasedWidget::invalidateLocal();
}


std::string OSDText::getType() const
{
	return "text";
}

template <typename IMAGE> BaseImage* OSDText::create(OutputSurface& output)
{
	if (!font.get()) {
		try {
			SystemFileContext context;
			string file = context.resolve(fontfile);
			int factor = getScaleFactor(output);
			font.reset(new TTFFont(file, size * factor));
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

TTFFont::TTFFont(std::string& filename, int ptsize)
{
	SDLTTF::instance(); // init library

	font = TTF_OpenFont(filename.c_str(), ptsize);
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

} // namespace openmsx
