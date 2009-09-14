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
	SDL_Rect newClip = { xn, yn, wn, hn };
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
	SubWidgetsMap::const_iterator it = subWidgetsMap.find(first);
	return it == subWidgetsMap.end() ? NULL : it->second->findSubWidget(last);
}

const OSDWidget* OSDWidget::findSubWidget(const std::string& name) const
{
	return const_cast<OSDWidget*>(this)->findSubWidget(name);
}

void OSDWidget::addWidget(std::auto_ptr<OSDWidget> widget)
{
	widget->setParent(this);
	OSDWidget* widgetPtr = widget.release();
	subWidgets.push_back(widgetPtr);
	subWidgetsMap[widgetPtr->getName()] = widgetPtr;
	resort();
}

void OSDWidget::deleteWidget(OSDWidget& widget)
{
	for (SubWidgets::iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		if (*it == &widget) {
			delete *it;
			subWidgets.erase(it);
			SubWidgetsMap::size_type existed =
				subWidgetsMap.erase(widget.getName());
			assert(existed); (void)existed;
			return;
		}
	}
	UNREACHABLE;
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
	result.insert("-mousecoord");
}

void OSDWidget::setProperty(const string& name, const TclObject& value)
{
	if ((name == "-type") || (name == "-mousecoord")) {
		throw CommandException("-type property is readonly");
	} else if (name == "-x") {
		x = value.getDouble();
	} else if (name == "-y") {
		y = value.getDouble();
	} else if (name == "-z") {
		double z2 = value.getDouble();
		if (z != z2) {
			z = z2;
			if (OSDWidget* parent = getParent()) {
				parent->resort();
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
	} else {
		throw CommandException("No such property: " + name);
	}
}

class DummyOutputRectangle : public OutputRectangle
{
public:
	DummyOutputRectangle(unsigned width_, unsigned height_)
		: width(width_), height(height_)
	{
	}
	virtual unsigned getOutputWidth()  const { return width;  }
	virtual unsigned getOutputHeight() const { return height; }
private:
	const unsigned width;
	const unsigned height;
};

void OSDWidget::getProperty(const string& name, TclObject& result) const
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
	paintSDL(output);

	std::auto_ptr<SDLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip.reset(new SDLScopedClip(output, x, y, w, h));
	}

	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->paintSDLRecursive(output);
	}
}

void OSDWidget::paintGLRecursive (OutputSurface& output)
{
	(void)output;
#if COMPONENT_GL
	paintGL(output);

	std::auto_ptr<GLScopedClip> scopedClip;
	if (clip) {
		int x, y, w, h;
		getBoundingBox(output, x, y, w, h);
		scopedClip.reset(new GLScopedClip(output, x, y, w, h));
	}

	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
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
	SDL_Surface* surface = SDL_GetVideoSurface();
	if (!surface) {
		throw CommandException(
			"-can't get mouse coordinates: no window visible");
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
	string pname = parentName + (parentName.empty() ? "" : ".");
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		string name = pname + (*it)->getName();
		result.insert(name);
		(*it)->listWidgetNames(name, result);
	}
}

} // namespace openmsx
