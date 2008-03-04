// $Id$

#include "OSDImageBasedWidget.hh"
#include "BaseImage.hh"
#include "OSDGUI.hh"
#include "Display.hh"
#include "Reactor.hh"
#include "GlobalCliComm.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <cassert>

using std::string;
using std::set;

namespace openmsx {

OSDImageBasedWidget::OSDImageBasedWidget(const OSDGUI& gui_, const string& name)
	: OSDWidget(name)
	, gui(gui_)
	, relx(0.0), rely(0.0)
	, x(0), y(0)
	, r(0), g(0), b(0), a(255)
	, error(false)
{
}

OSDImageBasedWidget::~OSDImageBasedWidget()
{
}

void OSDImageBasedWidget::getProperties(set<string>& result) const
{
	result.insert("-rgba");
	result.insert("-rgb");
	result.insert("-alpha");
	result.insert("-x");
	result.insert("-y");
	result.insert("-relx");
	result.insert("-rely");
	OSDWidget::getProperties(result);
}

void OSDImageBasedWidget::setProperty(const string& name, const string& value)
{
	if (name == "-rgba") {
		unsigned color = StringOp::stringToInt(value);
		r = (color >> 24) & 255;
		g = (color >> 16) & 255;
		b = (color >>  8) & 255;
		a = (color >>  0) & 255;
		invalidateLocal();
	} else if (name == "-rgb") {
		unsigned color = StringOp::stringToInt(value);
		r = (color >> 16) & 255;
		g = (color >>  8) & 255;
		b = (color >>  0) & 255;
		invalidateLocal();
	} else if (name == "-alpha") {
		// don't invalidate
		a = StringOp::stringToInt(value);
	} else if (name == "-x") {
		x = StringOp::stringToInt(value);
	} else if (name == "-y") {
		y = StringOp::stringToInt(value);
	} else if (name == "-relx") {
		relx = StringOp::stringToDouble(value);
	} else if (name == "-rely") {
		rely = StringOp::stringToDouble(value);
	} else {
		OSDWidget::setProperty(name, value);
	}
}

string OSDImageBasedWidget::getProperty(const string& name) const
{
	if (name == "-rgba") {
		unsigned color = (r << 24) | (g << 16) | (b << 8) | (a << 0);
		return StringOp::toString(color);
	} else if (name == "-rgb") {
		unsigned color = (r << 16) | (g << 8) | (b << 0);
		return StringOp::toString(color);
	} else if (name == "-alpha") {
		return StringOp::toString(unsigned(a));
	} else if (name == "-x") {
		return StringOp::toString(x);
	} else if (name == "-y") {
		return StringOp::toString(y);
	} else if (name == "-relx") {
		return StringOp::toString(relx);
	} else if (name == "-rely") {
		return StringOp::toString(rely);
	} else {
		return OSDWidget::getProperty(name);
	}
}

void OSDImageBasedWidget::invalidateLocal()
{
	error = false;
	image.reset();
}

void OSDImageBasedWidget::transformXY(const OutputSurface& output,
                                      int x, int y, double relx, double rely,
                                      int& outx, int& outy) const
{
	int width, height;
	if (image.get()) {
		width  = image->getWidth();
		height = image->getHeight();
	} else {
		assert(hasError());
		width  = 0;
		height = 0;
	}
	outx = x + getX() + int(relx * width);
	outy = y + getY() + int(rely * height);
	getParent()->transformXY(output, outx, outy, getRelX(), getRelY(),
	                         outx, outy);
}


void OSDImageBasedWidget::getTransformedXY(const OutputSurface& output,
                                           int& outx, int& outy) const
{
	const OSDWidget* parent = getParent();
	assert(parent);
	parent->transformXY(output, getX(), getY(), getRelX(), getRelY(),
	                    outx, outy);
}

void OSDImageBasedWidget::setError(const string& message)
{
	error = true;
	gui.getDisplay().getReactor().getGlobalCliComm().printWarning(message);
}

void OSDImageBasedWidget::paintSDL(OutputSurface& output)
{
	paint(output, false);
}

void OSDImageBasedWidget::paintGL(OutputSurface& output)
{
	paint(output, true);
}

void OSDImageBasedWidget::paint(OutputSurface& output, bool openGL)
{
	if (getAlpha() == 0) return;

	if (!image.get() && !hasError()) {
		try {
			if (openGL) {
				image.reset(createGL(output));
			} else {
				image.reset(createSDL(output));
			}
		} catch (MSXException& e) {
			setError(e.getMessage());
		}
	}
	if (image.get()) {
		int x, y;
		getTransformedXY(output, x, y);
		image->draw(x, y, getAlpha());
	}
}

} // namespace openmsx
