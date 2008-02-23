// $Id$

#include "OSDWidget.hh"
#include "OSDGUI.hh"
#include "CommandException.hh"
#include "StringOp.hh"

using std::string;
using std::set;

namespace openmsx {

OSDWidget::OSDWidget(OSDGUI& gui_)
	: gui(gui_)
	, z(0)
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

int OSDWidget::getZ() const
{
	return z;
}

void OSDWidget::getProperties(set<string>& result) const
{
	result.insert("-type");
	result.insert("-z");
}

void OSDWidget::setProperty(const string& name, const string& value)
{
	if (name == "-type") {
		throw CommandException("-type property is readonly");
	} else if (name == "-z") {
		z = StringOp::stringToInt(value);
		gui.resort();
	} else {
		throw CommandException("No such property: " + name);
	}
}

string OSDWidget::getProperty(const string& name) const
{
	if (name == "-type") {
		return getType();
	} else if (name == "-z") {
		return StringOp::toString(z);
	} else {
		throw CommandException("No such property: " + name);
	}
}

} // namespace openmsx
