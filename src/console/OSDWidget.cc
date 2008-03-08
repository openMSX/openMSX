// $Id$

#include "OSDWidget.hh"
#include "OutputSurface.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::set;

namespace openmsx {

OSDWidget::OSDWidget(const string& name_)
	: parent(NULL)
	, name(name_)
	, relx(0.0), rely(0.0)
	, x(0), y(0), z(0)
	, scaled(false)
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
}

void OSDWidget::setProperty(const string& name, const string& value)
{
	if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-x") {
		x = StringOp::stringToInt(value);
	} else if (name == "-y") {
		y = StringOp::stringToInt(value);
	} else if (name == "-z") {
		z = StringOp::stringToInt(value);
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
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->paintSDLRecursive(output);
	}
}

void OSDWidget::paintGLRecursive (OutputSurface& output)
{
	paintGL(output);
	for (SubWidgets::const_iterator it = subWidgets.begin();
	     it != subWidgets.end(); ++it) {
		(*it)->paintGLRecursive(output);
	}
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
                            int x, int y, double relx, double rely,
                            int& outx, int& outy) const
{
	int width, height;
	getWidthHeight(output, width, height);
	int factor = getScaleFactor(output);
	outx = x + factor * getX() + int(relx * width);
	outy = y + factor * getY() + int(rely * height);
	if (const OSDWidget* parent = getParent()) {
		parent->transformXY(output, outx, outy, getRelX(), getRelY(),
		                    outx, outy);
	}
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
