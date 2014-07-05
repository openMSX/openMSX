#include "OSDWidget.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "GLUtil.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <SDL.h>
#include <algorithm>
#include <limits>

using std::string;
using std::vector;
using std::shared_ptr;

namespace openmsx {

// intersect two rectangles
static void intersect(int xa, int ya, int wa, int ha,
                      int xb, int yb, int wb, int hb,
                      int& x, int& y, int& w, int& h)
{
	int x1 = std::max<int>(xa, xb);
	int y1 = std::max<int>(ya, yb);
	int x2 = std::min<int>(xa + wa, xb + wb);
	int y2 = std::min<int>(ya + ha, yb + hb);
	x = x1;
	y = y1;
	w = std::max(0, x2 - x1);
	h = std::max(0, y2 - y1);
}

////

static void normalize(int& x, int& w)
{
	if (w < 0) {
		w = -w;
		x -= w;
	}
}

class SDLScopedClip
{
public:
	SDLScopedClip(OutputSurface& output, int x, int y, int w, int h);
	~SDLScopedClip();
private:
	SDL_Surface* surface;
	SDL_Rect origClip;
};


SDLScopedClip::SDLScopedClip(OutputSurface& output, int x, int y, int w, int h)
	: surface(output.getSDLSurface())
{
	normalize(x, w); normalize(y, h);
	SDL_GetClipRect(surface, &origClip);

	int xn, yn, wn, hn;
	intersect(origClip.x, origClip.y, origClip.w, origClip.h,
	          x,  y,  w,  h,
	          xn, yn, wn, hn);
	SDL_Rect newClip = { Sint16(xn), Sint16(yn), Uint16(wn), Uint16(hn) };
	SDL_SetClipRect(surface, &newClip);
}

SDLScopedClip::~SDLScopedClip()
{
	SDL_SetClipRect(surface, &origClip);
}

////

#if COMPONENT_GL

class GLScopedClip
{
public:
	GLScopedClip(OutputSurface& output, int x, int y, int w, int h);
	~GLScopedClip();
private:
	GLint box[4]; // x, y, w, h;
	GLboolean wasEnabled;
};


GLScopedClip::GLScopedClip(OutputSurface& output, int x, int y, int w, int h)
{
	normalize(x, w); normalize(y, h);
	y = output.getHeight() - y - h; // openGL sets (0,0) in LOWER-left corner

	wasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	if (wasEnabled == GL_TRUE) {
		glGetIntegerv(GL_SCISSOR_BOX, box);
		int xn, yn, wn, hn;
		intersect(box[0], box[1], box[2], box[3],
		          x,  y,  w,  h,
		          xn, yn, wn, hn);
		glScissor(xn, yn, wn, hn);
	} else {
		glScissor(x, y, w, h);
		glEnable(GL_SCISSOR_TEST);
	}
}

GLScopedClip::~GLScopedClip()
{
	if (wasEnabled == GL_TRUE) {
		glScissor(box[0], box[1], box[2], box[3]);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
}

#endif

////

OSDWidget::OSDWidget(const string& name_)
	: parent(nullptr)
	, name(name_)
	, x(0.0), y(0.0), z(0.0)
	, relx(0.0), rely(0.0)
	, scaled(false)
	, clip(false)
	, suppressErrors(false)
{
}

OSDWidget::~OSDWidget()
{
}

const string& OSDWidget::getName() const
{
	return name;
}

OSDWidget* OSDWidget::getParent()
{
	return parent;
}

const OSDWidget* OSDWidget::getParent() const
{
	return parent;
}

void OSDWidget::setParent(OSDWidget* parent_)
{
	parent = parent_;
}

OSDWidget* OSDWidget::findSubWidget(string_ref name)
{
	if (name.empty()) {
		return this;
	}
	string_ref first, last;
	StringOp::splitOnFirst(name, '.', first, last);
	auto it = subWidgetsMap.find(first);
	return it == end(subWidgetsMap) ? nullptr : it->second->findSubWidget(last);
}

const OSDWidget* OSDWidget::findSubWidget(string_ref name) const
{
	return const_cast<OSDWidget*>(this)->findSubWidget(name);
}

void OSDWidget::addWidget(const shared_ptr<OSDWidget>& widget)
{
	widget->setParent(this);
	subWidgetsMap[widget->getName()] = widget.get();

	// Insert the new widget in the correct place (sorted on ascending Z)
	// heuristic: often we have either
	//  - many widgets with all the same Z
	//  - only a few total number of subwidgets (possibly with different Z)
	// In the former case we can simply append at the end. In the latter
	// case a linear search is probably faster than a binary search. Only
	// when there are many sub-widgets with not all the same Z (and not
	// created in sorted Z-order) a binary search would be faster.
	double z = widget->getZ();
	if (subWidgets.empty() || (subWidgets.back()->getZ() <= z)) {
		subWidgets.push_back(widget);
	} else {
		auto it = begin(subWidgets);
		while ((*it)->getZ() <= z) ++it;
		subWidgets.insert(it, widget);

	}
}

void OSDWidget::deleteWidget(OSDWidget& widget)
{
	string widgetName = widget.getName();
	for (auto it = begin(subWidgets); it != end(subWidgets); ++it) {
		if (it->get() == &widget) {
			subWidgets.erase(it);
			auto existed = subWidgetsMap.erase(widgetName);
			assert(existed); (void)existed;
			return;
		}
	}
	UNREACHABLE;
}

#ifdef DEBUG
struct AscendingZ {
	bool operator()(const shared_ptr<OSDWidget>& lhs,
	                const shared_ptr<OSDWidget>& rhs) const {
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
	double z = elem->getZ();
	auto it2 = it1;
	++it2;
	while ((it2 != end(subWidgets)) && ((*it2)->getZ() < z)) ++it2;
	// now move elements to correct position
	rotate(it1, it1 + 1, it2);
#ifdef DEBUG
	assert(std::is_sorted(begin(subWidgets), end(subWidgets), AscendingZ()));
#endif
}
void OSDWidget::resortDown(OSDWidget* elem)
{
	// z-coordinate was decreased, first search for new position
	auto it1 = begin(subWidgets);
	double z = elem->getZ();
	while ((*it1)->getZ() <= z) {
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
	assert(std::is_sorted(begin(subWidgets), end(subWidgets), AscendingZ()));
#endif
}

vector<string_ref> OSDWidget::getProperties() const
{
	static const char* const vals[] = {
		"-type", "-x", "-y", "-z", "-relx", "-rely", "-scaled",
		"-clip", "-mousecoord", "-suppressErrors",
	};
	return vector<string_ref>(std::begin(vals), std::end(vals));
}

void OSDWidget::setProperty(
	Interpreter& interp, string_ref name, const TclObject& value)
{
	if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-mousecoord") {
		throw CommandException("-mousecoord property is readonly");
	} else if (name == "-x") {
		x = value.getDouble(interp);
	} else if (name == "-y") {
		y = value.getDouble(interp);
	} else if (name == "-z") {
		double z2 = value.getDouble(interp);
		if (z != z2) {
			bool up = z2 > z; // was z increased?
			z = z2;
			if (OSDWidget* parent = getParent()) {
				// TODO no need for a full sort: instead remove and re-insert in the correct place
				if (up) {
					parent->resortUp(this);
				} else {
					parent->resortDown(this);
				}
			}
		}
	} else if (name == "-relx") {
		relx = value.getDouble(interp);
	} else if (name == "-rely") {
		rely = value.getDouble(interp);
	} else if (name == "-scaled") {
		bool scaled2 = value.getBoolean(interp);
		if (scaled != scaled2) {
			scaled = scaled2;
			invalidateRecursive();
		}
	} else if (name == "-clip") {
		clip = value.getBoolean(interp);
	} else if (name == "-suppressErrors") {
		suppressErrors = value.getBoolean(interp);
	} else {
		throw CommandException("No such property: " + name);
	}
}

void OSDWidget::getProperty(string_ref name, TclObject& result) const
{
	if (name == "-type") {
		result.setString(getType());
	} else if (name == "-x") {
		result.setDouble(x);
	} else if (name == "-y") {
		result.setDouble(y);
	} else if (name == "-z") {
		result.setDouble(z);
	} else if (name == "-relx") {
		result.setDouble(relx);
	} else if (name == "-rely") {
		result.setDouble(rely);
	} else if (name == "-scaled") {
		result.setBoolean(scaled);
	} else if (name == "-clip") {
		result.setBoolean(clip);
	} else if (name == "-mousecoord") {
		double x, y;
		getMouseCoord(x, y);
		result.addListElements({x, y});
	} else if (name == "-suppressErrors") {
		result.setBoolean(suppressErrors);
	} else {
		throw CommandException("No such property: " + name);
	}
}

double OSDWidget::getRecursiveFadeValue() const
{
	return 1.0; // fully opaque
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
	if (const OSDWidget* parent = getParent()) {
		return parent->needSuppressErrors();
	}
	return false;
}

void OSDWidget::paintSDLRecursive(OutputSurface& output)
{
	paintSDL(output);

	std::unique_ptr<SDLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip = make_unique<SDLScopedClip>(output, x, y, w, h);
	}

	// Iterate over a copy because drawing could cause errors (e.g. can't
	// load font or image), those error are (indirectly via CliComm) passed
	// to a Tcl callback and that callback can destroy or create extra
	// widgets.
	auto copy = subWidgets;
	for (auto& s : copy) {
		s->paintSDLRecursive(output);
	}
}

void OSDWidget::paintGLRecursive (OutputSurface& output)
{
	(void)output;
#if COMPONENT_GL
	paintGL(output);

	std::unique_ptr<GLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip = make_unique<GLScopedClip>(output, x, y, w, h);
	}

	auto copy = subWidgets;
	for (auto& s : copy) {
		s->paintGLRecursive(output);
	}
#endif
}

int OSDWidget::getScaleFactor(const OutputRectangle& output) const
{
	if (scaled) {
		return output.getOutputWidth() / 320;;
	} else if (getParent()) {
		return getParent()->getScaleFactor(output);
	} else {
		return 1;
	}
}

void OSDWidget::transformXY(const OutputRectangle& output,
                            double x, double y, double relx, double rely,
                            double& outx, double& outy) const
{
	double width, height;
	getWidthHeight(output, width, height);
	int factor = getScaleFactor(output);
	outx = x + factor * getX() + relx * width;
	outy = y + factor * getY() + rely * height;
	if (const OSDWidget* parent = getParent()) {
		parent->transformXY(output, outx, outy, getRelX(), getRelY(),
		                    outx, outy);
	}
}

void OSDWidget::transformReverse(
	const OutputRectangle& output, double x, double y,
	double& outx, double& outy) const
{
	if (const OSDWidget* parent = getParent()) {
		parent->transformReverse(output, x, y, x, y);
		double width, height;
		parent->getWidthHeight(output, width, height);
		int factor = getScaleFactor(output);
		outx = x - (getRelX() * width ) - (getX() * factor);
		outy = y - (getRelY() * height) - (getY() * factor);
	} else {
		outx = x;
		outy = y;
	}
}

void OSDWidget::getMouseCoord(double& outx, double& outy) const
{
	if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
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
		outx = std::numeric_limits<double>::infinity();
		outy = std::numeric_limits<double>::infinity();
		return;
	}

	SDL_Surface* surface = SDL_GetVideoSurface();
	if (!surface) {
		throw CommandException(
			"Can't get mouse coordinates: no window visible");
	}
	DummyOutputRectangle output(surface->w, surface->h);

	int mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	transformReverse(output, mouseX, mouseY, outx, outy);

	double width, height;
	getWidthHeight(output, width, height);
	if ((width == 0) || (height == 0)) {
		throw CommandException(
			"-can't get mouse coordinates: "
			"widget has zero width or height");
	}
	outx /= width;
	outy /= height;
}

void OSDWidget::getBoundingBox(const OutputRectangle& output,
                               int& x, int& y, int& w, int& h)
{
	double x1, y1, x2, y2;
	transformXY(output, 0.0, 0.0, 0.0, 0.0, x1, y1);
	transformXY(output, 0.0, 0.0, 1.0, 1.0, x2, y2);
	x = int(x1 + 0.5);
	y = int(y1 + 0.5);
	w = int(x2 - x1 + 0.5);
	h = int(y2 - y1 + 0.5);
}

void OSDWidget::listWidgetNames(const string& parentName, vector<string>& result) const
{
	string pname = parentName;
	if (!pname.empty()) pname += '.';
	for (auto& s : subWidgets) {
		string name = pname + s->getName();
		result.push_back(name);
		s->listWidgetNames(name, result);
	}
}

} // namespace openmsx
