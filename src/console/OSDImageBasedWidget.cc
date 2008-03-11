// $Id$

#include "OSDImageBasedWidget.hh"
#include "OSDGUI.hh"
#include "BaseImage.hh"
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
	} else {
		return OSDWidget::getProperty(name);
	}
}

void OSDImageBasedWidget::invalidateLocal()
{
	error = false;
	image.reset();
}

void OSDImageBasedWidget::getWidthHeight(const OutputSurface& /*output*/,
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

void OSDImageBasedWidget::getTransformedXY(const OutputSurface& output,
                                           double& outx, double& outy) const
{
	const OSDWidget* parent = getParent();
	assert(parent);
	int factor = getScaleFactor(output);
	double x = factor * getX();
	double y = factor * getY();
	parent->transformXY(output, x, y, getRelX(), getRelY(), outx, outy);
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
	// Note: Even when getAlpha() == 0 we still create the image:
	//    It may be needed to get the dimensions to be able to position
	//    child widgets.
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
	if ((getAlpha() != 0) && image.get()) {
		double x, y;
		getTransformedXY(output, x, y);
		image->draw(int(x), int(y), getAlpha());
	}
}

} // namespace openmsx
