// $Id$

#include <sstream>
#include "FrameSkipSetting.hh"
#include "CommandException.hh"

using std::ostringstream;


namespace openmsx {

FrameSkipSetting::FrameSkipSetting()
	: Setting<FrameSkip>(
		"frameskip", "set the amount of frameskip",
		FrameSkip(false, 0)
		)
{
	type = "0 - 100 / auto";
}

string FrameSkipSetting::getValueString() const
{
	if (getValue().isAutoFrameSkip()) {
		return "auto";
	} else {
		ostringstream out;
		out << getValue().getFrameSkip();
		return out.str();
	}
}

void FrameSkipSetting::setValueString(
	const string &valueString)
{
	if (valueString == "auto") {
		setValue(FrameSkip(true, getValue().getFrameSkip()));
	} else {
		int tmp = strtol(valueString.c_str(), NULL, 0);
		if ((0 <= tmp) && (tmp <= 100)) {
			setValue(FrameSkip(false, tmp));
		} else {
			throw CommandException("Not a valid value");
		}
	}
}

// TODO: Find out if there is a cleaner way to instantiate this method.
void FrameSkipSetting::setValue(const FrameSkip &newValue)
{
	Setting<FrameSkip>::setValue(newValue);
}

} // namespace openmsx
