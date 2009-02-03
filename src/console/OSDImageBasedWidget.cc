// $Id$

#include "OSDImageBasedWidget.hh"
#include "OSDGUI.hh"
#include "BaseImage.hh"
#include "Display.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "Timer.hh"
#include <cassert>

using std::string;
using std::set;

namespace openmsx {

OSDImageBasedWidget::OSDImageBasedWidget(const OSDGUI& gui_, const string& name)
	: OSDWidget(name)
	, gui(gui_)
	, setFadeTime(0)
	, fadePeriod(0.0)
	, fadeTarget(255)
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
	result.insert("-fadePeriod");
	result.insert("-fadeTarget");
	OSDWidget::getProperties(result);
}

void OSDImageBasedWidget::setProperty(const string& name, const string& value)
{
	if (name == "-rgba") {
		unsigned color = StringOp::stringToUint(value);
		r = (color >> 24) & 255;
		g = (color >> 16) & 255;
		b = (color >>  8) & 255;
		setAlpha((color >>  0) & 255);
		invalidateLocal();
	} else if (name == "-rgb") {
		unsigned color = StringOp::stringToUint(value);
		r = (color >> 16) & 255;
		g = (color >>  8) & 255;
		b = (color >>  0) & 255;
		invalidateLocal();
	} else if (name == "-alpha") {
		// don't invalidate
		setAlpha(StringOp::stringToUint(value));
	} else if (name == "-fadePeriod") {
		unsigned long long now = Timer::getTime();
		setAlpha(getAlpha(now), now); // recalculate current (faded) alpha
		fadePeriod = StringOp::stringToDouble(value);
	} else if (name == "-fadeTarget") {
		unsigned long long now = Timer::getTime();
		setAlpha(getAlpha(now), now); // recalculate current (faded) alpha
		fadeTarget = StringOp::stringToUint(value);
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
		return StringOp::toString(unsigned(getAlpha()));
	} else if (name == "-fadePeriod") {
		return StringOp::toString(fadePeriod);
	} else if (name == "-fadeTarget") {
		return StringOp::toString(unsigned(fadeTarget));
	} else {
		return OSDWidget::getProperty(name);
	}
}

bool OSDImageBasedWidget::isFading() const
{
	return (a != fadeTarget) && (fadePeriod != 0.0);
}

byte OSDImageBasedWidget::getAlpha() const
{
	if (!isFading()) {
		// optimization
		return a;
	}
	return getAlpha(Timer::getTime());
}

byte OSDImageBasedWidget::getAlpha(unsigned long long now) const
{
	assert(now >= setFadeTime);
	if (fadePeriod == 0.0) {
		return a;
	}

	int diff = int(now - setFadeTime); // int should be big enough
	double ratio = diff / (1000000.0 * fadePeriod);
	int dAlpha = int(256.0 * ratio);
	if (a < fadeTarget) {
		int tmpAlpha = a + dAlpha;
		if (tmpAlpha >= fadeTarget) {
			a = fadeTarget;
			return a;
		}
		return tmpAlpha;
	} else {
		int tmpAlpha = a - dAlpha;
		if (tmpAlpha <= fadeTarget) {
			a = fadeTarget;
			return a;
		}
		return tmpAlpha;
	}
}

void OSDImageBasedWidget::setAlpha(byte alpha)
{
	setAlpha(alpha, Timer::getTime());
}

void OSDImageBasedWidget::setAlpha(byte alpha, unsigned long long now)
{
	a = alpha;
	setFadeTime = now;
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
	gui.getDisplay().getCliComm().printWarning(message);
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
	if (isFading()) {
		gui.refresh();
	}
}

} // namespace openmsx
