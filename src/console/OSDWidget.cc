// $Id$

#include "OSDWidget.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "GLUtil.hh"
#include <SDL.h>
#include <algorithm>
#include <cassert>

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
	SDL_GetClipRect(surface, &origClip);

	int xn, yn, wn, hn;
	intersect(origClip.x, origClip.y, origClip.w, origClip.h,
	          x,  y,  w,  h,
	          xn, yn, wn, hn);
	SDL_Rect newClip = { xn, yn, wn, hn };
	SDL_SetClipRect(surface, &newClip);
}

SDLScopedClip::~SDLScopedClip()
{
	SDL_SetClipRect(surface, &origClip);
}

////

#ifdef COMPONENT_GL

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
	wasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, &xo);

	int xn, yn, wn, hn;
	intersect(xo, yo, wo, ho,
	          x,  y,  w,  h,
	          xn, yn, wn, hn);
	glScissor(xn, output.getHeight() - yn - hn, wn, hn);
	glEnable(GL_SCISSOR_TEST);
}

GLScopedClip::~GLScopedClip()
{
	glScissor(xo, yo, wo, ho);
	if (wasEnabled == GL_FALSE) {
		glDisable(GL_SCISSOR_TEST);
	}
}

#endif

////

OSDWidget::OSDWidget(const string& name_)
	: parent(NULL)
	, name(name_)
	, x(0.0), y(0.0), z(0.0)
	, relx(0.0), rely(0.0)
	, scaled(false)
	, clip(false)
{
}

OSDWidget::~OSDWidget()
{
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		delete *it;
	}
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

OSDWidget* OSDWidget::findSubWidget(const string& name)
{
	if (name.empty()) {
		return this;
	}
	string first, last;
	StringOp::splitOnFirst(name, ".", first, last);
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		if ((*it)->getName() == first) {
			return (*it)->findSubWidget(last);
		}
	}
	return NULL;
}

const OSDWidget* OSDWidget::findSubWidget(const std::string& name) const
{
	return const_cast<OSDWidget*>(this)->findSubWidget(name);
}

void OSDWidget::addWidget(std::auto_ptr<OSDWidget> widget)
{
	widget->setParent(this);
	subWidgets.push_back(widget.release());
	resort();
}

void OSDWidget::deleteWidget(OSDWidget& widget)
{
	for (SubWidgets::iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		if (*it == &widget) {
			delete *it;
			subWidgets.erase(it);
			return;
		}
	}
	assert(false);
}

struct AscendingZ {
	bool operator()(const OSDWidget* lhs, const OSDWidget* rhs) const {
		return lhs->getZ() < rhs->getZ();
	}
};
void OSDWidget::resort()
{
	std::stable_sort(subWidgets.begin(), subWidgets.end(), AscendingZ());
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
}

void OSDWidget::setProperty(const string& name, const string& value)
{
	if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-x") {
		x = StringOp::stringToDouble(value);
	} else if (name == "-y") {
		y = StringOp::stringToDouble(value);
	} else if (name == "-z") {
		z = StringOp::stringToDouble(value);
		if (OSDWidget* parent = getParent()) {
			parent->resort();
		}
	} else if (name == "-relx") {
		relx = StringOp::stringToDouble(value);
	} else if (name == "-rely") {
		rely = StringOp::stringToDouble(value);
	} else if (name == "-scaled") {
		scaled = StringOp::stringToBool(value);
		invalidateRecursive();
	} else if (name == "-clip") {
		clip = StringOp::stringToBool(value);
	} else {
		throw CommandException("No such property: " + name);
	}
}

string OSDWidget::getProperty(const string& name) const
{
	if (name == "-type") {
		return getType();
	} else if (name == "-x") {
		return StringOp::toString(x);
	} else if (name == "-y") {
		return StringOp::toString(y);
	} else if (name == "-z") {
		return StringOp::toString(z);
	} else if (name == "-relx") {
		return StringOp::toString(relx);
	} else if (name == "-rely") {
		return StringOp::toString(rely);
	} else if (name == "-scaled") {
		return scaled ? "true" : "false";
	} else if (name == "-clip") {
		return clip ? "true" : "false";
	} else {
		throw CommandException("No such property: " + name);
	}
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

void OSDWidget::paintSDLRecursive(OutputSurface& output)
{
	std::auto_ptr<SDLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip.reset(new SDLScopedClip(output, x, y, w, h));
	}

	paintSDL(output);
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->paintSDLRecursive(output);
	}
}

void OSDWidget::paintGLRecursive (OutputSurface& output)
{
#ifdef COMPONENT_GL
	std::auto_ptr<GLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip.reset(new GLScopedClip(output, x, y, w, h));
	}

	paintGL(output);
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->paintGLRecursive(output);
	}
#endif
}

int OSDWidget::getScaleFactor(const OutputSurface& output) const
{
	if (scaled) {
		return output.getWidth() / 320;;
	} else if (getParent()) {
		return getParent()->getScaleFactor(output);
	} else {
		return 1;
	}
}

void OSDWidget::transformXY(const OutputSurface& output,
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

void OSDWidget::getBoundingBox(const OutputSurface& output,
                               int& x, int& y, int& w, int& h)
{
	double x1, y1, x2, y2;
	transformXY(output, 0.0, 0.0, 0.0, 0.0, x1, y1);
	transformXY(output, 0.0, 0.0, 1.0, 1.0, x2, y2);
	x = int(x1);
	y = int(y1);
	w = int(x2) - int(x1);
	h = int(y2) - int(y1);
}

void OSDWidget::listWidgetNames(const string& parentName, set<string>& result) const
{
	string pname = parentName + (parentName.empty() ? "" : ".");
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		string name = pname + (*it)->getName();
		result.insert(name);
		(*it)->listWidgetNames(name, result);
	}
}

} // namespace openmsx
