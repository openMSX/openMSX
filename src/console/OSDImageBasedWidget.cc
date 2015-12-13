#include "OSDImageBasedWidget.hh"
#include "OSDTopWidget.hh"
#include "OSDGUI.hh"
#include "BaseImage.hh"
#include "OutputSurface.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "Timer.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;
using namespace gl;

namespace openmsx {

OSDImageBasedWidget::OSDImageBasedWidget(OSDGUI& gui_, const TclObject& name_)
	: OSDWidget(name_)
	, gui(gui_)
	, startFadeTime(0)
	, fadePeriod(0.0)
	, fadeTarget(1.0)
	, startFadeValue(1.0)
	, error(false)
{
	for (auto i : xrange(4)) {
		rgba[i] = 0x000000ff;
	}
}

OSDImageBasedWidget::~OSDImageBasedWidget()
{
}

vector<string_ref> OSDImageBasedWidget::getProperties() const
{
	auto result = OSDWidget::getProperties();
	static const char* const vals[] = {
		"-rgba", "-rgb", "-alpha", "-fadePeriod", "-fadeTarget",
		"-fadeCurrent",
	};
	result.insert(end(result), std::begin(vals), std::end(vals));
	return result;
}

static void get4(Interpreter& interp, const TclObject& value, unsigned* result)
{
	unsigned len = value.getListLength(interp);
	if (len == 4) {
		for (auto i : xrange(4)) {
			result[i] = value.getListIndex(interp, i).getInt(interp);
		}
	} else if (len == 1) {
		unsigned val = value.getInt(interp);
		for (auto i : xrange(4)) {
			result[i] = val;
		}
	} else {
		throw CommandException("Expected either 1 or 4 values.");
	}
}
void OSDImageBasedWidget::setProperty(
	Interpreter& interp, string_ref propName, const TclObject& value)
{
	if (propName == "-rgba") {
		unsigned newRGBA[4];
		get4(interp, value, newRGBA);
		setRGBA(newRGBA);
	} else if (propName == "-rgb") {
		unsigned newRGB[4];
		get4(interp, value, newRGB);
		unsigned newRGBA[4];
		for (auto i : xrange(4)) {
			newRGBA[i] = (rgba[i]          & 0x000000ff) |
			             ((newRGB[i] << 8) & 0xffffff00);
		}
		setRGBA(newRGBA);
	} else if (propName == "-alpha") {
		unsigned newAlpha[4];
		get4(interp, value, newAlpha);
		unsigned newRGBA[4];
		for (auto i : xrange(4)) {
			newRGBA[i] = (rgba[i]     & 0xffffff00) |
			             (newAlpha[i] & 0x000000ff);
		}
		setRGBA(newRGBA);
	} else if (propName == "-fadePeriod") {
		updateCurrentFadeValue();
		fadePeriod = value.getDouble(interp);
	} else if (propName == "-fadeTarget") {
		updateCurrentFadeValue();
		fadeTarget = std::max(0.0, std::min(1.0 , value.getDouble(interp)));
	} else if (propName == "-fadeCurrent") {
		startFadeValue = std::max(0.0, std::min(1.0, value.getDouble(interp)));
		startFadeTime = Timer::getTime();
	} else {
		OSDWidget::setProperty(interp, propName, value);
	}
}

void OSDImageBasedWidget::setRGBA(const unsigned newRGBA[4])
{
	if ((rgba[0] == newRGBA[0]) &&
	    (rgba[1] == newRGBA[1]) &&
	    (rgba[2] == newRGBA[2]) &&
	    (rgba[3] == newRGBA[3])) {
		// not changed
		return;
	}
	invalidateLocal();
	for (auto i : xrange(4)) {
		rgba[i] = newRGBA[i];
	}
}

static void set4(const unsigned rgba[4], unsigned mask, unsigned shift, TclObject& result)
{
	if ((rgba[0] == rgba[1]) && (rgba[0] == rgba[2]) && (rgba[0] == rgba[3])) {
		result.setInt((rgba[0] & mask) >> shift);
	} else {

		for (auto i : xrange(4)) {
			result.addListElement(int((rgba[i] & mask) >> shift));
		}
	}
}
void OSDImageBasedWidget::getProperty(string_ref propName, TclObject& result) const
{
	if (propName == "-rgba") {
		set4(rgba, 0xffffffff, 0, result);
	} else if (propName == "-rgb") {
		set4(rgba, 0xffffff00, 8, result);
	} else if (propName == "-alpha") {
		set4(rgba, 0x000000ff, 0, result);
	} else if (propName == "-fadePeriod") {
		result.setDouble(fadePeriod);
	} else if (propName == "-fadeTarget") {
		result.setDouble(fadeTarget);
	} else if (propName == "-fadeCurrent") {
		result.setDouble(getCurrentFadeValue());
	} else {
		OSDWidget::getProperty(propName, result);
	}
}

static bool constantAlpha(const unsigned rgba[4])
{
	return ((rgba[0] & 0xff) == (rgba[1] & 0xff)) &&
	       ((rgba[0] & 0xff) == (rgba[2] & 0xff)) &&
	       ((rgba[0] & 0xff) == (rgba[3] & 0xff));
}
bool OSDImageBasedWidget::hasConstantAlpha() const
{
	return constantAlpha(rgba);
}

float OSDImageBasedWidget::getRecursiveFadeValue() const
{
	return getParent()->getRecursiveFadeValue() * getCurrentFadeValue();
}

bool OSDImageBasedWidget::isFading() const
{
	return (startFadeValue != fadeTarget) && (fadePeriod != 0.0f);
}

float OSDImageBasedWidget::getCurrentFadeValue() const
{
	if (!isFading()) {
		return startFadeValue;
	}
	return getCurrentFadeValue(Timer::getTime());
}

float OSDImageBasedWidget::getCurrentFadeValue(uint64_t now) const
{
	assert(now >= startFadeTime);

	int diff = int(now - startFadeTime); // int should be big enough
	assert(fadePeriod != 0.0f);
	float delta = diff / (1000000.0f * fadePeriod);
	if (startFadeValue < fadeTarget) {
		float tmp = startFadeValue + delta;
		if (tmp >= fadeTarget) {
			startFadeValue = fadeTarget;
			return startFadeValue;
		}
		return tmp;
	} else {
		float tmp = startFadeValue - delta;
		if (tmp <= fadeTarget) {
			startFadeValue = fadeTarget;
			return startFadeValue;
		}
		return tmp;
	}
}

void OSDImageBasedWidget::updateCurrentFadeValue()
{
	auto now = Timer::getTime();
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

vec2 OSDImageBasedWidget::getTransformedPos(const OutputRectangle& output) const
{
	return getParent()->transformPos(
		output, float(getScaleFactor(output)) * getPos(), getRelPos());
}

void OSDImageBasedWidget::setError(string message)
{
	error = true;

	// The suppressErrors property only exists to break an infinite loop
	// when an error occurs (e.g. couldn't load font) while displaying the
	// error message on the OSD system.
	// The difficulty in detecting this loop is that it's not a recursive
	// loop, but each iteration takes one frame: on the CliComm Tcl callback,
	// the OSD widgets get created, but only the next frame, when this new
	// widget is actually drawn the next error occurs.
	if (!needSuppressErrors()) {
		gui.getTopWidget().queueError(std::move(message));
	}
}

void OSDImageBasedWidget::paintSDL(OutputSurface& output)
{
	paint(output, false);
}

void OSDImageBasedWidget::paintGL(OutputSurface& output)
{
	paint(output, true);
}

void OSDImageBasedWidget::createImage(OutputRectangle& output)
{
	if (!image && !hasError()) {
		try {
			if (gui.isOpenGL()) {
				image = createGL(output);
			} else {
				image = createSDL(output);
			}
		} catch (MSXException& e) {
			setError(e.getMessage());
		}
	}
}

void OSDImageBasedWidget::paint(OutputSurface& output, bool openGL)
{
	// Note: Even when alpha == 0 we still create the image:
	//    It may be needed to get the dimensions to be able to position
	//    child widgets.
	assert(openGL == gui.isOpenGL()); (void)openGL;
	createImage(output);

	byte fadedAlpha = getFadedAlpha();
	if ((fadedAlpha != 0) && image) {
		ivec2 drawPos = round(getTransformedPos(output));
		image->draw(output, drawPos, fadedAlpha);
	}
	if (isFading()) {
		gui.refresh();
	}
}

} // namespace openmsx
