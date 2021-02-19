#include "OSDWidget.hh"
#include "SDLOutputSurface.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "GLUtil.hh"
#include "checked_cast.hh"
#include "ranges.hh"
#include "stl.hh"
#include <SDL.h>
#include <array>
#include <limits>
#include <optional>

using std::string;
using std::string_view;
using std::vector;
using std::unique_ptr;
using namespace gl;

namespace openmsx {

// intersect two rectangles
struct IntersectResult { int x, y, w, h; };
static constexpr IntersectResult intersect(int xa, int ya, int wa, int ha,
                                           int xb, int yb, int wb, int hb)
{
	int x1 = std::max<int>(xa, xb);
	int y1 = std::max<int>(ya, yb);
	int x2 = std::min<int>(xa + wa, xb + wb);
	int y2 = std::min<int>(ya + ha, yb + hb);
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

class SDLScopedClip
{
public:
	SDLScopedClip(OutputSurface& output, vec2 xy, vec2 wh);
	~SDLScopedClip();
	SDLScopedClip(const SDLScopedClip&) = delete;
	SDLScopedClip& operator=(const SDLScopedClip&) = delete;
private:
	SDL_Renderer* renderer;
	std::optional<SDL_Rect> origClip;
};


SDLScopedClip::SDLScopedClip(OutputSurface& output, vec2 xy, vec2 wh)
	: renderer(checked_cast<SDLOutputSurface&>(output).getSDLRenderer())
{
	ivec2 i_xy = round(xy); auto [x, y] = i_xy;
	ivec2 i_wh = round(wh); auto [w, h] = i_wh;
	normalize(x, w); normalize(y, h);

	auto [xn, yn, wn, hn] = [&, x=x, y=y, w=w, h=h] {
		if (SDL_RenderIsClipEnabled(renderer)) {
			origClip.emplace();
			SDL_RenderGetClipRect(renderer, &*origClip);

			return intersect(origClip->x, origClip->y, origClip->w, origClip->h,
			                 x,  y,  w,  h);
		} else {
			return IntersectResult{x, y, w, h};
		}
	}();
	SDL_Rect newClip = { Sint16(xn), Sint16(yn), Uint16(wn), Uint16(hn) };
	SDL_RenderSetClipRect(renderer, &newClip);
}

SDLScopedClip::~SDLScopedClip()
{
	SDL_RenderSetClipRect(renderer, origClip ? &*origClip : nullptr);
}

////

#if COMPONENT_GL

class GLScopedClip
{
public:
	GLScopedClip(OutputSurface& output, vec2 xy, vec2 wh);
	~GLScopedClip();
	GLScopedClip(const GLScopedClip&) = delete;
	GLScopedClip& operator=(const GLScopedClip&) = delete;
private:
	std::optional<std::array<GLint, 4>> origClip; // x, y, w, h;
};


GLScopedClip::GLScopedClip(OutputSurface& output, vec2 xy, vec2 wh)
{
	auto& [x, y] = xy;
	auto& [w, h] = wh;
	normalize(x, w); normalize(y, h);
	y = output.getLogicalHeight() - y - h; // openGL sets (0,0) in LOWER-left corner

	// transform view-space coordinates to clip-space coordinates
	vec2 scale = output.getViewScale();
	auto [ix, iy] = round(xy * scale) + output.getViewOffset();
	auto [iw, ih] = round(wh * scale);

	if (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE) {
		origClip.emplace();
		glGetIntegerv(GL_SCISSOR_BOX, origClip->data());
		auto [xn, yn, wn, hn] = intersect(
			(*origClip)[0], (*origClip)[1], (*origClip)[2], (*origClip)[3],
			ix, iy, iw, ih);
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

#endif

////

OSDWidget::OSDWidget(Display& display_, TclObject name_)
	: display(display_)
	, parent(nullptr)
	, name(std::move(name_))
	, z(0.0)
	, scaled(false)
	, clip(false)
	, suppressErrors(false)
{
}

void OSDWidget::addWidget(unique_ptr<OSDWidget> widget)
{
	widget->setParent(this);

	// Insert the new widget in the correct place (sorted on ascending Z)
	// heuristic: often we have either
	//  - many widgets with all the same Z
	//  - only a few total number of subwidgets (possibly with different Z)
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
	auto it = rfind_if_unguarded(subWidgets,
		[&](const std::unique_ptr<OSDWidget>& p) { return p.get() == &widget; });
	subWidgets.erase(it);
}

#ifdef DEBUG
struct AscendingZ {
	bool operator()(const unique_ptr<OSDWidget>& lhs,
	                const unique_ptr<OSDWidget>& rhs) const {
		return lhs->getZ() < rhs->getZ();
	}
};
#endif
void OSDWidget::resortUp(OSDWidget* elem)
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
	assert(ranges::is_sorted(subWidgets, AscendingZ()));
#endif
}
void OSDWidget::resortDown(OSDWidget* elem)
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
	assert(ranges::is_sorted(subWidgets, AscendingZ()));
#endif
}

vector<string_view> OSDWidget::getProperties() const
{
	static constexpr const char* const vals[] = {
		"-type", "-x", "-y", "-z", "-relx", "-rely", "-scaled",
		"-clip", "-mousecoord", "-suppressErrors",
	};
	return to_vector<string_view>(vals);
}

void OSDWidget::setProperty(
	Interpreter& interp, string_view propName, const TclObject& value)
{
	if (propName == "-type") {
		throw CommandException("-type property is readonly");
	} else if (propName == "-mousecoord") {
		throw CommandException("-mousecoord property is readonly");
	} else if (propName == "-x") {
		pos[0] = value.getDouble(interp);
	} else if (propName == "-y") {
		pos[1] = value.getDouble(interp);
	} else if (propName == "-z") {
		float z2 = value.getDouble(interp);
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
		relPos[0] = value.getDouble(interp);
	} else if (propName == "-rely") {
		relPos[1] = value.getDouble(interp);
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

void OSDWidget::getProperty(string_view propName, TclObject& result) const
{
	if (propName == "-type") {
		result = getType();
	} else if (propName == "-x") {
		result = pos[0];
	} else if (propName == "-y") {
		result = pos[1];
	} else if (propName == "-z") {
		result = z;
	} else if (propName == "-relx") {
		result = relPos[0];
	} else if (propName == "-rely") {
		result = relPos[1];
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

void OSDWidget::invalidateChildren()
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

void OSDWidget::paintSDLRecursive(OutputSurface& output)
{
	paintSDL(output);

	std::optional<SDLScopedClip> scopedClip;
	if (clip) {
		vec2 clipPos, size;
		getBoundingBox(output, clipPos, size);
		scopedClip.emplace(output, clipPos, size);
	}

	for (auto& s : subWidgets) {
		s->paintSDLRecursive(output);
	}
}

void OSDWidget::paintGLRecursive (OutputSurface& output)
{
	(void)output;
#if COMPONENT_GL
	paintGL(output);

	std::optional<GLScopedClip> scopedClip;
	if (clip) {
		vec2 clipPos, size;
		getBoundingBox(output, clipPos, size);
		scopedClip.emplace(output, clipPos, size);
	}

	for (auto& s : subWidgets) {
		s->paintGLRecursive(output);
	}
#endif
}

int OSDWidget::getScaleFactor(const OutputSurface& output) const
{
	if (scaled) {
		return output.getLogicalWidth() / 320;;
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
		// click on the reversebar. This will also block the OSD mouse
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
	if ((size[0] == 0.0f) || (size[1] == 0.0f)) {
		throw CommandException(
			"-can't get mouse coordinates: "
			"widget has zero width or height");
	}
	return out / size;
}

void OSDWidget::getBoundingBox(const OutputSurface& output,
                               vec2& bbPos, vec2& bbSize) const
{
	vec2 topLeft     = transformPos(output, vec2(), vec2(0.0f));
	vec2 bottomRight = transformPos(output, vec2(), vec2(1.0f));
	bbPos  = topLeft;
	bbSize = bottomRight - topLeft;
}

} // namespace openmsx
