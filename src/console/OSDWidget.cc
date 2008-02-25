// $Id$

#include "OSDWidget.hh"
#include "OSDGUI.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include <cassert>

using std::string;
using std::set;

namespace openmsx {

OSDWidget::OSDWidget(OSDGUI& gui_)
	: gui(gui_)
	, x(0), y(0), z(0)
	, r(0), g(0), b(0), a(255)
{
	static unsigned count = 0;
	id = ++count;
}

OSDWidget::~OSDWidget()
{
}

string OSDWidget::getName() const
{
	return "OSD#" + StringOp::toString(id);
}

void OSDWidget::getProperties(set<string>& result) const
{
	result.insert("-type");
	result.insert("-x");
	result.insert("-y");
	result.insert("-z");
	result.insert("-rgba");
	result.insert("-rgb");
	result.insert("-alpha");
}

void OSDWidget::setProperty(const string& name, const string& value)
{
	if (name == "-x") {
		x = StringOp::stringToInt(value);
	} else if (name == "-y") {
		y = StringOp::stringToInt(value);
	} else if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-z") {
		z = StringOp::stringToInt(value);
		gui.resort();
	} else if ((name == "-rgba") || (name == "-rgb") || (name == "-alpha")) {
		// must be handled by sub-class because here we don't know
		// when to call invalidate()
		assert(false);
	} else {
		throw CommandException("No such property: " + name);
	}
}

bool OSDWidget::setRGBA(const string& value, bool* rgbChanged, bool* alphaChanged)
{
	unsigned color = StringOp::stringToInt(value);
	byte r2 = (color >> 24) & 255;
	byte g2 = (color >> 16) & 255;
	byte b2 = (color >>  8) & 255;
	byte a2 = (color >>  0) & 255;
	bool rgbChanged2   = (r != r2) || (g != g2) || (b != b2);
	bool alphaChanged2 = (a != a2);
	if (rgbChanged)   *rgbChanged   = rgbChanged2;
	if (alphaChanged) *alphaChanged = alphaChanged2;
	r = r2; g = g2; b = b2; a = a2;
	return rgbChanged2 || alphaChanged2;
}

bool OSDWidget::setRGB(const string& value)
{
	unsigned color = StringOp::stringToInt(value);
	byte r2 = (color >> 16) & 255;
	byte g2 = (color >>  8) & 255;
	byte b2 = (color >>  0) & 255;
	bool colorChanged = (r != r2) || (g != g2) || (b != b2);
	r = r2; g = g2; b = b2;
	return colorChanged;
}

bool OSDWidget::setAlpha(const string& value)
{
	byte a2 = StringOp::stringToInt(value);
	bool alphaChanged = (a != a2);
	a = a2;
	return alphaChanged;
}


string OSDWidget::getProperty(const string& name) const
{
	if (name == "-type") {
		return getType();
	} else if (name == "-z") {
		return StringOp::toString(z);
	} else if (name == "-rgba") {
		unsigned color = (r << 24) | (g << 16) | (b << 8) | (a << 0);
		return StringOp::toString(color);
	} else if (name == "-rgb") {
		unsigned color = (r << 16) | (g << 8) | (b << 0);
		return StringOp::toString(color);
	} else if (name == "-alpha") {
		return StringOp::toString(unsigned(a));
	} else {
		throw CommandException("No such property: " + name);
	}
}

} // namespace openmsx
