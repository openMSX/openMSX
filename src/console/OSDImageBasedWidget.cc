// $Id$

#include "OSDImageBasedWidget.hh"
#include "OSDGUI.hh"
#include "BaseImage.hh"
#include "OutputSurface.hh"
#include "Display.hh"
#include "CliComm.hh"
#include "TclObject.hh"
#include "MSXException.hh"
#include "Timer.hh"
#include <cassert>

using std::string;
using std::set;

namespace openmsx {

OSDImageBasedWidget::OSDImageBasedWidget(const OSDGUI& gui_, const string& name)
	: OSDWidget(name)
	, gui(gui_)
	, startFadeTime(0)
	, fadePeriod(0.0)
	, fadeTarget(1.0)
	, startFadeValue(1.0)
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
	result.insert("-fadeCurrent");
	OSDWidget::getProperties(result);
}

void OSDImageBasedWidget::setProperty(const string& name, const TclObject& value)
{
	if (name == "-rgba") {
		unsigned color = value.getInt();
		byte r2 = (color >> 24) & 255;
		byte g2 = (color >> 16) & 255;
		byte b2 = (color >>  8) & 255;
		if ((r != r2) || (g != g2) || (b != b2)) {
			r = r2; g = g2; b = b2;
			invalidateLocal();
		}
		a = (color >>  0) & 255;
	} else if (name == "-rgb") {
		unsigned color = value.getInt();
		byte r2 = (color >> 16) & 255;
		byte g2 = (color >>  8) & 255;
		byte b2 = (color >>  0) & 255;
		if ((r != r2) || (g != g2) || (b != b2)) {
			r = r2; g = g2; b = b2;
			invalidateLocal();
		}
	} else if (name == "-alpha") {
		// don't invalidate
		a = value.getInt();
	} else if (name == "-fadePeriod") {
		updateCurrentFadeValue();
		fadePeriod = value.getDouble();
	} else if (name == "-fadeTarget") {
		updateCurrentFadeValue();
		fadeTarget = std::max(0.0, std::min(1.0 , value.getDouble()));
	} else if (name == "-fadeCurrent") {
		startFadeValue = std::max(0.0, std::min(1.0, value.getDouble()));
		startFadeTime = Timer::getTime();
	} else {
		OSDWidget::setProperty(name, value);
	}
}

void OSDImageBasedWidget::getProperty(const string& name, TclObject& result) const
{
	if (name == "-rgba") {
		unsigned color = (r << 24) | (g << 16) | (b << 8) | (a << 0);
		result.setInt(color);
	} else if (name == "-rgb") {
		unsigned color = (r << 16) | (g << 8) | (b << 0);
		result.setInt(color);
	} else if (name == "-alpha") {
		result.setInt(a);
	} else if (name == "-fadePeriod") {
		result.setDouble(fadePeriod);
	} else if (name == "-fadeTarget") {
		result.setDouble(fadeTarget);
	} else if (name == "-fadeCurrent") {
		result.setDouble(getCurrentFadeValue());
	} else {
		OSDWidget::getProperty(name, result);
	}
}

byte OSDImageBasedWidget::getFadedAlpha() const
{
	return byte(a * getRecursiveFadeValue());
}

double OSDImageBasedWidget::getRecursiveFadeValue() const
{
	return getParent()->getRecursiveFadeValue() * getCurrentFadeValue();
}

bool OSDImageBasedWidget::isFading() const
{
	return (startFadeValue != fadeTarget) && (fadePeriod != 0.0);
}

double OSDImageBasedWidget::getCurrentFadeValue() const
{
	if (!isFading()) {
		return startFadeValue;
	}
	return getCurrentFadeValue(Timer::getTime());
}

double OSDImageBasedWidget::getCurrentFadeValue(unsigned long long now) const
{
	assert(now >= startFadeTime);

	int diff = int(now - startFadeTime); // int should be big enough
	assert(fadePeriod != 0.0);
	double delta = diff / (1000000.0 * fadePeriod);
	if (startFadeValue < fadeTarget) {
		double tmp = startFadeValue + delta;
		if (tmp >= fadeTarget) {
			startFadeValue = fadeTarget;
			return startFadeValue;
		}
		return tmp;
	} else {
		double tmp = startFadeValue - delta;
		if (tmp <= fadeTarget) {
			startFadeValue = fadeTarget;
			return startFadeValue;
		}
		return tmp;
	}
}

void OSDImageBasedWidget::updateCurrentFadeValue()
{
	unsigned long long now = Timer::getTime();
	if (isFading()) {
		startFadeValue = getCurrentFadeValue(now);
	}
	startFadeTime = now;
}

void OSDImageBasedWidget::invalidateLocal()
{
	error = false;
	image.reset();
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
	// Note: Even when alpha == 0 we still create the image:
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
	byte fadedAlpha = getFadedAlpha();
	if ((fadedAlpha != 0) && image.get()) {
		double x, y;
		getTransformedXY(output, x, y);
		image->draw(output, int(x + 0.5), int(y + 0.5), fadedAlpha);
	}
	if (isFading()) {
		gui.refresh();
	}
}

} // namespace openmsx
