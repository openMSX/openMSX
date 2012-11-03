// $Id$

#include "OSDWidget.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "GLUtil.hh"
#include "openmsx.hh"
#include "unreachable.hh"
#include <SDL.h>
#include <algorithm>
#include <limits>

using std::string;
using std::set;

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
	: surface(output.getSDLWorkSurface())
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
	GLint xo, yo, wo, ho; // order is important
	GLboolean wasEnabled;
};


GLScopedClip::GLScopedClip(OutputSurface& output, int x, int y, int w, int h)
{
	normalize(x, w); normalize(y, h);
	y = output.getHeight() - y - h; // openGL sets (0,0) in LOWER-left corner

	wasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	if (wasEnabled == GL_TRUE) {
		glGetIntegerv(GL_SCISSOR_BOX, &xo);
		int xn, yn, wn, hn;
		intersect(xo, yo, wo, ho,
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
		glScissor(xo, yo, wo, ho);
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
	SubWidgetsMap::const_iterator it = subWidgetsMap.find(first);
	return it == subWidgetsMap.end() ? nullptr : it->second->findSubWidget(last);
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
		SubWidgets::iterator it = subWidgets.begin();
		while ((*it)->getZ() <= z) ++it;
		subWidgets.insert(it, widget);

	}
}

void OSDWidget::deleteWidget(OSDWidget& widget)
{
	string widgetName = widget.getName();
	for (SubWidgets::iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		if (it->get() == &widget) {
			subWidgets.erase(it);
			SubWidgetsMap::size_type existed =
				subWidgetsMap.erase(widgetName);
			assert(existed); (void)existed;
			return;
		}
	}
	UNREACHABLE;
}

#ifdef DEBUG
// Note: this function has the same name and the same functionality as the
// std::is_sorted function in c++11. Once we switch to a c++11 compiler (or
// enable c++11 mode) this function can be removed. Vc++ has c++11 mode enabled
// by default, this currently causes an ambiguous function call, as a temporary
// fix we have to explicitly qualify all calls to this function.
template<class ForwardIterator, class StrictWeakOrdering>
bool is_sorted(ForwardIterator first, ForwardIterator last,
               StrictWeakOrdering comp)
{
	if (first == last) return true;
	ForwardIterator next = first;
	++next;
	while (next != last) {
		if (comp(*next, *first)) return false;
		++first; ++next;
	}
	return true;
}
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
	SubWidgets::iterator it1 = subWidgets.begin();
	while (it1->get() != elem) ++it1;
	// next search for the position were it belongs
	double z = elem->getZ();
	SubWidgets::iterator it2 = it1;
	++it2;
	while ((it2 != subWidgets.end()) && ((*it2)->getZ() < z)) ++it2;
	// now move elements to correct position
	rotate(it1, it1 + 1, it2);
#ifdef DEBUG
	assert(openmsx::is_sorted(subWidgets.begin(), subWidgets.end(), AscendingZ()));
#endif
}
void OSDWidget::resortDown(OSDWidget* elem)
{
	// z-coordinate was decreased, first search for new position
	SubWidgets::iterator it1 = subWidgets.begin();
	double z = elem->getZ();
	while ((*it1)->getZ() <= z) {
		++it1;
		if (it1 == subWidgets.end()) return;
	}
	// next search for the elements current position
	SubWidgets::iterator it2 = it1;
	if ((it2 != subWidgets.begin()) && ((it2 - 1)->get() == elem)) return;
	while (it2->get() != elem) ++it2;
	// now move elements to correct position
	rotate(it1, it2, it2 + 1);
#ifdef DEBUG
	assert(openmsx::is_sorted(subWidgets.begin(), subWidgets.end(), AscendingZ()));
#endif
}

void OSDWidget::getProperties(set<string>& result) const
{
	result.insert("-type");
	result.insert("-x");
	result.insert("-y");
	result.insert("-z");
	result.insert("-relx");
	result.insert("-rely");
	result.insert("-scaled");
	result.insert("-clip");
	result.insert("-mousecoord");
	result.insert("-suppressErrors");
}

void OSDWidget::setProperty(string_ref name, const TclObject& value)
{
	if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-mousecoord") {
		throw CommandException("-mousecoord property is readonly");
	} else if (name == "-x") {
		x = value.getDouble();
	} else if (name == "-y") {
		y = value.getDouble();
	} else if (name == "-z") {
		double z2 = value.getDouble();
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
		relx = value.getDouble();
	} else if (name == "-rely") {
		rely = value.getDouble();
	} else if (name == "-scaled") {
		bool scaled2 = value.getBoolean();
		if (scaled != scaled2) {
			scaled = scaled2;
			invalidateRecursive();
		}
	} else if (name == "-clip") {
		clip = value.getBoolean();
	} else if (name == "-suppressErrors") {
		suppressErrors = value.getBoolean();
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
		result.addListElement(x);
		result.addListElement(y);
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
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->invalidateRecursive();
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
		scopedClip.reset(new SDLScopedClip(output, x, y, w, h));
	}

	// Iterate over a copy because drawing could cause errors (e.g. can't
	// load font or image), those error are (indirectly via CliComm) passed
	// to a Tcl callback and that callback can destroy or create extra
	// widgets.
	SubWidgets copy(subWidgets);
	for (SubWidgets::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		(*it)->paintSDLRecursive(output);
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
		scopedClip.reset(new GLScopedClip(output, x, y, w, h));
	}

	SubWidgets copy(subWidgets);
	for (SubWidgets::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		(*it)->paintGLRecursive(output);
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

void OSDWidget::listWidgetNames(const string& parentName, set<string>& result) const
{
	string pname = parentName;
	if (!pname.empty()) pname += '.';
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		string name = pname + (*it)->getName();
		result.insert(name);
		(*it)->listWidgetNames(name, result);
	}
}

} // namespace openmsx
