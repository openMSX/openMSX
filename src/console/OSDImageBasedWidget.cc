#include "OSDImageBasedWidget.hh"
#include "OSDTopWidget.hh"
#include "OSDGUI.hh"
#include "BaseImage.hh"
#include "Display.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "Timer.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"
#include "view.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>

using namespace gl;

namespace openmsx {

OSDImageBasedWidget::OSDImageBasedWidget(Display& display_, const TclObject& name_)
	: OSDWidget(display_, name_)
	, startFadeTime(0)
	, fadePeriod(0.0)
	, fadeTarget(1.0)
	, startFadeValue(1.0)
	, error(false)
{
	ranges::fill(rgba, 0x000000ff); // black, opaque
}

OSDImageBasedWidget::~OSDImageBasedWidget() = default;

[[nodiscard]] static std::array<uint32_t, 4> get4(Interpreter& interp, const TclObject& value)
{
	std::array<uint32_t, 4> result;
	auto len = value.getListLength(interp);
	if (len == 4) {
		for (auto i : xrange(4)) {
			result[i] = value.getListIndex(interp, i).getInt(interp);
		}
	} else if (len == 1) {
		ranges::fill(result, value.getInt(interp));
	} else {
		throw CommandException("Expected either 1 or 4 values.");
	}
	return result;
}
void OSDImageBasedWidget::setProperty(
	Interpreter& interp, std::string_view propName, const TclObject& value)
{
	if (propName == "-rgba") {
		std::array<uint32_t, 4> newRGBA = get4(interp, value);
		setRGBA(newRGBA);
	} else if (propName == "-rgb") {
		std::array<uint32_t, 4> newRGB = get4(interp, value);
		std::array<uint32_t, 4> newRGBA;
		for (auto i : xrange(4)) {
			newRGBA[i] = (rgba[i]          & 0x000000ff) |
			             ((newRGB[i] << 8) & 0xffffff00);
		}
		setRGBA(newRGBA);
	} else if (propName == "-alpha") {
		std::array<uint32_t, 4> newAlpha = get4(interp, value);
		std::array<uint32_t, 4> newRGBA;
		for (auto i : xrange(4)) {
			newRGBA[i] = (rgba[i]     & 0xffffff00) |
			             (newAlpha[i] & 0x000000ff);
		}
		setRGBA(newRGBA);
	} else if (propName == "-fadePeriod") {
		updateCurrentFadeValue();
		fadePeriod = value.getFloat(interp);
	} else if (propName == "-fadeTarget") {
		updateCurrentFadeValue();
		fadeTarget = std::clamp(value.getFloat(interp), 0.0f, 1.0f);
	} else if (propName == "-fadeCurrent") {
		startFadeValue = std::clamp(value.getFloat(interp), 0.0f, 1.0f);
		startFadeTime = Timer::getTime();
	} else {
		OSDWidget::setProperty(interp, propName, value);
	}
}

void OSDImageBasedWidget::setRGBA(std::span<const uint32_t, 4> newRGBA)
{
	if (ranges::equal(rgba, newRGBA)) {
		return; // not changed
	}
	invalidateLocal();
	ranges::copy(newRGBA, rgba);
}

static void set4(std::span<const uint32_t, 4> rgba, uint32_t mask, unsigned shift, TclObject& result)
{
	if (ranges::all_equal(rgba)) {
		result = (rgba[0] & mask) >> shift;
	} else {
		result.addListElements(view::transform(xrange(4), [&](auto i) {
			return int((rgba[i] & mask) >> shift);
		}));
	}
}
void OSDImageBasedWidget::getProperty(std::string_view propName, TclObject& result) const
{
	if (propName == "-rgba") {
		set4(rgba, 0xffffffff, 0, result);
	} else if (propName == "-rgb") {
		set4(rgba, 0xffffff00, 8, result);
	} else if (propName == "-alpha") {
		set4(rgba, 0x000000ff, 0, result);
	} else if (propName == "-fadePeriod") {
		result = fadePeriod;
	} else if (propName == "-fadeTarget") {
		result = fadeTarget;
	} else if (propName == "-fadeCurrent") {
		result = getCurrentFadeValue();
	} else {
		OSDWidget::getProperty(propName, result);
	}
}

bool OSDImageBasedWidget::hasConstantAlpha() const
{
	return ranges::all_equal(rgba, [](auto c) { return c & 0xff; });
}

float OSDImageBasedWidget::getRecursiveFadeValue() const
{
	return getParent()->getRecursiveFadeValue() * getCurrentFadeValue();
}

bool OSDImageBasedWidget::isVisible() const
{
	return (getFadedAlpha() != 0) || isRecursiveFading();
}

bool OSDImageBasedWidget::isFading() const
{
	return (startFadeValue != fadeTarget) && (fadePeriod != 0.0f);
}

bool OSDImageBasedWidget::isRecursiveFading() const
{
	if (isFading()) return true;
	return getParent()->isRecursiveFading();
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

	auto diff = narrow<int>(now - startFadeTime); // int should be big enough
	assert(fadePeriod != 0.0f);
	float delta = narrow_cast<float>(diff) / (1000000.0f * fadePeriod);
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

vec2 OSDImageBasedWidget::getTransformedPos(const OutputSurface& output) const
{
	return getParent()->transformPos(
		output, float(getScaleFactor(output)) * getPos(), getRelPos());
}

void OSDImageBasedWidget::setError(std::string message)
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
		getDisplay().getOSDGUI().getTopWidget().queueError(std::move(message));
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

void OSDImageBasedWidget::createImage(OutputSurface& output)
{
	if (!image && !hasError()) {
		try {
			if (getDisplay().getOSDGUI().isOpenGL()) {
				image = createGL(output);
			} else {
				image = createSDL(output);
			}
		} catch (MSXException& e) {
			setError(std::move(e).getMessage());
		}
	}
}

void OSDImageBasedWidget::paint(OutputSurface& output, bool openGL)
{
	// Note: Even when alpha == 0 we still create the image:
	//    It may be needed to get the dimensions to be able to position
	//    child widgets.
	assert(openGL == getDisplay().getOSDGUI().isOpenGL()); (void)openGL;
	createImage(output);

	auto fadedAlpha = getFadedAlpha();
	if ((fadedAlpha != 0) && image) {
		ivec2 drawPos = round(getTransformedPos(output));
		image->draw(output, drawPos, fadedAlpha);
	}
	if (isRecursiveFading()) {
		getDisplay().getOSDGUI().refresh();
	}
}

} // namespace openmsx
