// $Id$

#include "OSDText.hh"
#include "TTFFont.hh"
#include "SDLImage.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "components.hh"
#include <cassert>
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

void OSDText::setProperty(const string& name, const TclObject& value)
{
	if (name == "-text") {
		string val = value.getString();
		if (text != val) {
			text = val;
			// note: don't invalidate font (don't reopen font file)
			OSDImageBasedWidget::invalidateLocal();
			invalidateChildren();
		}
	} else if (name == "-font") {
		string val = value.getString();
		if (fontfile != val) {
			SystemFileContext context;
			CommandController* controller = NULL; // ok for SystemFileContext
			string file = context.resolve(*controller, val);
			if (!FileOperations::isRegularFile(file)) {
				throw CommandException("Not a valid font file: " + val);
			}
			fontfile = val;
			invalidateRecursive();
		}
	} else if (name == "-size") {
		int size2 = value.getInt();
		if (size != size2) {
			size = size2;
			invalidateRecursive();
		}
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

void OSDText::getProperty(const string& name, TclObject& result) const
{
	if (name == "-text") {
		result.setString(text);
	} else if (name == "-font") {
		result.setString(fontfile);
	} else if (name == "-size") {
		result.setInt(size);
	} else {
		OSDImageBasedWidget::getProperty(name, result);
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

void OSDText::getWidthHeight(const OutputSurface& /*output*/,
                             double& width, double& height) const
{
	if (image.get()) {
		width  = image->getWidth();
		height = image->getHeight();
	} else {
		// we don't know the dimensions, must be because of an error
		assert(hasError());
		width  = 0;
		height = 0;
	}
}

template <typename IMAGE> BaseImage* OSDText::create(OutputSurface& output)
{
	if (text.empty()) {
		return new IMAGE(0, 0, byte(0));
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
