// $Id$

#include "OSDText.hh"
#include "TTFFont.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;

namespace openmsx {

OSDText::OSDText(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, fontfile("skins/Vera.ttf.gz")
	, size(12)
{
}

OSDText::~OSDText()
{
}

void OSDText::getProperties(std::set<string>& result) const
{
	result.insert("-text");
	result.insert("-font");
	result.insert("-size");
	OSDImageBasedWidget::getProperties(result);
}

void OSDText::setProperty(const string& name, const string& value)
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

string OSDText::getProperty(const string& name) const
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


string OSDText::getType() const
{
	return "text";
}

template <typename IMAGE> BaseImage* OSDText::create(OutputSurface& output)
{
	if (text.empty()) {
		return new IMAGE(0, 0, 0);
	}
	if (!font.get()) {
		try {
			SystemFileContext context;
			CommandController* controller = NULL; // ok for SystemFileContext
			string file = context.resolve(*controller, fontfile);
			int ptSize = size * getScaleFactor(output);
			font.reset(new TTFFont(file, ptSize));
		} catch (MSXException& e) {
			throw MSXException("Couldn't open font: " + e.getMessage());
		}
	}
	try {
		SDL_Surface* surface = font->render(
			text, getRed(), getGreen(), getBlue());
		return new IMAGE(surface);
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
#else
	(void)&output;
	return NULL;
#endif
}

} // namespace openmsx
