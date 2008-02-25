// $Id$

#include "OSDText.hh"
#include "SDLImage.hh"
#include "FileContext.hh"
#include "MSXException.hh"
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

OSDText::OSDText(OSDGUI& gui)
	: OSDWidget(gui)
	, fontfile("notepad.ttf")
	, size(12)
{
}

void OSDText::getProperties(std::set<std::string>& result) const
{
	result.insert("-text");
	result.insert("-font");
	result.insert("-size");
	OSDWidget::getProperties(result);
}

void OSDText::setProperty(const std::string& name, const std::string& value)
{
	if (name == "-text") {
		text = value;
		invalidate();
	} else if (name == "-font") {
		fontfile = value;
		invalidate();
		font.reset();
	} else if (name == "-size") {
		size = StringOp::stringToInt(value);
		invalidate();
		font.reset();
	} else if (name == "-rgba") {
		bool rgbChanged;
		setRGBA(value, &rgbChanged);
		if (rgbChanged) {
			invalidate();
		}
	} else if (name == "-rgb") {
		if (setRGB(value)) {
			invalidate();
		}
	} else if (name == "-alpha") {
		setAlpha(value);
		// don't invalidate
	} else {
		OSDWidget::setProperty(name, value);
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
		return OSDWidget::getProperty(name);
	}
}

std::string OSDText::getType() const
{
	return "text";
}

template <typename IMAGE> void OSDText::paint(OutputSurface& output)
{
	if (!image.get() && !hasError()) {
		if (!font.get()) {
			try {
				SystemFileContext context;
				string file = context.resolve(fontfile);
				font.reset(new TTFFont(file, size));
			} catch (MSXException& e) {
				setError("Couldn't open font: " + e.getMessage());
			}
		}
		if (font.get()) {
			try {
				SDL_Surface* surface = font->render(
					text, getRed(), getGreen(), getBlue());
				image.reset(new IMAGE(output, surface));
			} catch (MSXException& e) {
				setError("Couldn't render text: " + e.getMessage());
			}
		}
	}
	if (image.get()) {
		image->draw(getX(), getY(), getAlpha());
	}
}

void OSDText::paintSDL(OutputSurface& output)
{
	paint<SDLImage>(output);
}

void OSDText::paintGL(OutputSurface& output)
{
#ifdef COMPONENT_GL
	paint<GLImage>(output);
#endif
}

void OSDText::invalidateInternal()
{
	image.reset();
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
