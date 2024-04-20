#include "OSDWidget.hh"

#include "CommandException.hh"
#include "Display.hh"
#include "GLUtil.hh"
#include "OutputSurface.hh"
#include "TclObject.hh"
#include "VideoSystem.hh"

#include "narrow.hh"
#include "stl.hh"

#include <SDL.h>

#include <array>
#include <limits>
#include <optional>

using namespace gl;

namespace openmsx {

// intersect two rectangles
struct Rectangle { int x, y, w, h; };
static constexpr Rectangle intersect(const Rectangle& a, const Rectangle& b)
{
	int x1 = std::max<int>(a.x, b.x);
	int y1 = std::max<int>(a.y, b.y);
	int x2 = std::min<int>(a.x + a.w, b.x + b.w);
	int y2 = std::min<int>(a.y + a.h, b.y + b.h);
	int w = std::max(0, x2 - x1);
	int h = std::max(0, y2 - y1);
	return {x1, y1, w, h};
}

////
template<typename T>
static constexpr void normalize(T& x, T& w)
{
	if (w < 0) {
		w = -w;
		x -= w;
	}
}


class GLScopedClip
{
public:
	GLScopedClip(const OutputSurface& output, vec2 xy, vec2 wh);
	GLScopedClip(const GLScopedClip&) = delete;
	GLScopedClip(GLScopedClip&&) = delete;
	GLScopedClip& operator=(const GLScopedClip&) = delete;
	GLScopedClip& operator=(GLScopedClip&&) = delete;
	~GLScopedClip();
private:
	std::optional<std::array<GLint, 4>> origClip; // x, y, w, h;
};


GLScopedClip::GLScopedClip(const OutputSurface& output, vec2 xy, vec2 wh)
{
	auto& [x, y] = xy;
	auto& [w, h] = wh;
	normalize(x, w); normalize(y, h);
	y = narrow_cast<float>(output.getLogicalHeight()) - y - h; // openGL sets (0,0) in LOWER-left corner

	// transform view-space coordinates to clip-space coordinates
	vec2 scale = output.getViewScale();
	auto [ix, iy] = round(xy * scale) + output.getViewOffset();
	auto [iw, ih] = round(wh * scale);

	if (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE) {
		origClip.emplace();
		glGetIntegerv(GL_SCISSOR_BOX, origClip->data());
		auto [xn, yn, wn, hn] = intersect(
			Rectangle{(*origClip)[0], (*origClip)[1], (*origClip)[2], (*origClip)[3]},
			Rectangle{ix, iy, iw, ih});
		glScissor(xn, yn, wn, hn);
	} else {
		glScissor(ix, iy, iw, ih);
		glEnable(GL_SCISSOR_TEST);
	}
}

GLScopedClip::~GLScopedClip()
{
	if (origClip) {
		glScissor((*origClip)[0], (*origClip)[1], (*origClip)[2], (*origClip)[3]);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
}

////

OSDWidget::OSDWidget(Display& display_, TclObject name_)
	: display(display_)
	, name(std::move(name_))
{
}

void OSDWidget::addWidget(std::unique_ptr<OSDWidget> widget)
{
	widget->setParent(this);

	// Insert the new widget in the correct place (sorted on ascending Z)
	// heuristic: often we have either
	//  - many widgets with all the same Z
	//  - only a few total number of subWidgets (possibly with different Z)
	// In the former case we can simply append at the end. In the latter
	// case a linear search is probably faster than a binary search. Only
	// when there are many sub-widgets with not all the same Z (and not
	// created in sorted Z-order) a binary search would be faster.
	float widgetZ = widget->getZ();
	if (subWidgets.empty() || (subWidgets.back()->getZ() <= widgetZ)) {
		subWidgets.push_back(std::move(widget));
	} else {
		auto it = begin(subWidgets);
		while ((*it)->getZ() <= widgetZ) ++it;
		subWidgets.insert(it, std::move(widget));

	}
}

void OSDWidget::deleteWidget(OSDWidget& widget)
{
	auto it = rfind_unguarded(subWidgets, &widget,
	                          [](const auto& p) { return p.get(); });
	subWidgets.erase(it);
}

void OSDWidget::resortUp(const OSDWidget* elem)
{
	// z-coordinate was increased, first search for elements current position
	auto it1 = begin(subWidgets);
	while (it1->get() != elem) ++it1;
	// next search for the position were it belongs
	float elemZ = elem->getZ();
	auto it2 = it1;
	++it2;
	while ((it2 != end(subWidgets)) && ((*it2)->getZ() < elemZ)) ++it2;
	// now move elements to correct position
	rotate(it1, it1 + 1, it2);
#ifdef DEBUG
	assert(ranges::is_sorted(subWidgets, {}, &OSDWidget::getZ));
#endif
}
void OSDWidget::resortDown(const OSDWidget* elem)
{
	// z-coordinate was decreased, first search for new position
	auto it1 = begin(subWidgets);
	float elemZ = elem->getZ();
	while ((*it1)->getZ() <= elemZ) {
		++it1;
		if (it1 == end(subWidgets)) return;
	}
	// next search for the elements current position
	auto it2 = it1;
	if ((it2 != begin(subWidgets)) && ((it2 - 1)->get() == elem)) return;
	while (it2->get() != elem) ++it2;
	// now move elements to correct position
	rotate(it1, it2, it2 + 1);
#ifdef DEBUG
	assert(ranges::is_sorted(subWidgets, {}, &OSDWidget::getZ));
#endif
}

void OSDWidget::setProperty(
	Interpreter& interp, std::string_view propName, const TclObject& value)
{
	if (propName == "-type") {
		throw CommandException("-type property is readonly");
	} else if (propName == "-mousecoord") {
		throw CommandException("-mousecoord property is readonly");
	} else if (propName == "-x") {
		pos.x = value.getFloat(interp);
	} else if (propName == "-y") {
		pos.y = value.getFloat(interp);
	} else if (propName == "-z") {
		float z2 = value.getFloat(interp);
		if (z != z2) {
			bool up = z2 > z; // was z increased?
			z = z2;
			if (auto* p = getParent()) {
				// TODO no need for a full sort: instead remove and re-insert in the correct place
				if (up) {
					p->resortUp(this);
				} else {
					p->resortDown(this);
				}
			}
		}
	} else if (propName == "-relx") {
		relPos.x = value.getFloat(interp);
	} else if (propName == "-rely") {
		relPos.y = value.getFloat(interp);
	} else if (propName == "-scaled") {
		bool scaled2 = value.getBoolean(interp);
		if (scaled != scaled2) {
			scaled = scaled2;
			invalidateRecursive();
		}
	} else if (propName == "-clip") {
		clip = value.getBoolean(interp);
	} else if (propName == "-suppressErrors") {
		suppressErrors = value.getBoolean(interp);
	} else {
		throw CommandException("No such property: ", propName);
	}
}

void OSDWidget::getProperty(std::string_view propName, TclObject& result) const
{
	if (propName == "-type") {
		result = getType();
	} else if (propName == "-x") {
		result = pos.x;
	} else if (propName == "-y") {
		result = pos.y;
	} else if (propName == "-z") {
		result = z;
	} else if (propName == "-relx") {
		result = relPos.x;
	} else if (propName == "-rely") {
		result = relPos.y;
	} else if (propName == "-scaled") {
		result = scaled;
	} else if (propName == "-clip") {
		result = clip;
	} else if (propName == "-mousecoord") {
		auto [x, y] = getMouseCoord();
		result.addListElement(x, y);
	} else if (propName == "-suppressErrors") {
		result = suppressErrors;
	} else {
		throw CommandException("No such property: ", propName);
	}
}

float OSDWidget::getRecursiveFadeValue() const
{
	return 1.0f; // fully opaque
}

void OSDWidget::invalidateRecursive()
{
	invalidateLocal();
	invalidateChildren();
}

void OSDWidget::invalidateChildren() const
{
	for (auto& s : subWidgets) {
		s->invalidateRecursive();
	}
}

bool OSDWidget::needSuppressErrors() const
{
	if (suppressErrors) return true;
	if (const auto* p = getParent()) {
		return p->needSuppressErrors();
	}
	return false;
}

void OSDWidget::paintRecursive(OutputSurface& output)
{
	paint(output);

	std::optional<GLScopedClip> scopedClip;
	if (clip) {
		auto [clipPos, size] = getBoundingBox(output);
		scopedClip.emplace(output, clipPos, size);
	}

	for (auto& s : subWidgets) {
		s->paintRecursive(output);
	}
}

int OSDWidget::getScaleFactor(const OutputSurface& output) const
{
	if (scaled) {
		return output.getLogicalWidth() / 320;
	} else if (getParent()) {
		return getParent()->getScaleFactor(output);
	} else {
		return 1;
	}
}

vec2 OSDWidget::transformPos(const OutputSurface& output,
                             vec2 trPos, vec2 trRelPos) const
{
	vec2 out = trPos
	         + (float(getScaleFactor(output)) * getPos())
		 + (trRelPos * getSize(output));
	if (const auto* p = getParent()) {
		out = p->transformPos(output, out, getRelPos());
	}
	return out;
}

vec2 OSDWidget::transformReverse(const OutputSurface& output, vec2 trPos) const
{
	if (const auto* p = getParent()) {
		trPos = p->transformReverse(output, trPos);
		return trPos
		       - (getRelPos() * p->getSize(output))
		       - (getPos() * float(getScaleFactor(output)));
	} else {
		return trPos;
	}
}

vec2 OSDWidget::getMouseCoord() const
{
	auto& videoSystem = getDisplay().getVideoSystem();
	if (!videoSystem.getCursorEnabled()) {
		// Host cursor is not visible. Return dummy mouse coords for
		// the OSD cursor position.
		// The reason for doing this is that otherwise (e.g. when using
		// the mouse in an MSX program) it's possible to accidentally
		// click on the reverse bar. This will also block the OSD mouse
		// in other Tcl scripts (e.g. vampier's nemesis script), but
		// almost always those scripts will also not be useful when the
		// host mouse cursor is not visible.
		//
		// We need to return coordinates that lay outside any
		// reasonable range. Initially we returned (NaN, NaN). But for
		// some reason that didn't work on dingoo: Dingoo uses
		// softfloat, in c++ NaN seems to behave as expected, but maybe
		// there's a problem on the tcl side? Anyway, when we return
		// +inf instead of NaN it does work.
		return vec2(std::numeric_limits<float>::infinity());
	}

	auto* output = getDisplay().getOutputSurface();
	if (!output) {
		throw CommandException(
			"Can't get mouse coordinates: no window visible");
	}

	vec2 out = transformReverse(*output, vec2(videoSystem.getMouseCoord()));
	vec2 size = getSize(*output);
	if ((size.x == 0.0f) || (size.y == 0.0f)) {
		throw CommandException(
			"-can't get mouse coordinates: "
			"widget has zero width or height");
	}
	return out / size;
}

OSDWidget::BoundingBox OSDWidget::getBoundingBox(const OutputSurface& output) const
{
	vec2 topLeft     = transformPos(output, vec2(), vec2(0.0f));
	vec2 bottomRight = transformPos(output, vec2(), vec2(1.0f));
	return {topLeft, bottomRight - topLeft};
}

} // namespace openmsx
