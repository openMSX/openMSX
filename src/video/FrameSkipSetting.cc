// $Id$

#include <sstream>
#include "FrameSkipSetting.hh"


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
	if (value.isAutoFrameSkip()) {
		return "auto";
	} else {
		ostringstream out;
		out << value.getFrameSkip();
		return out.str();
	}
}

void FrameSkipSetting::setValueString(
	const string &valueString)
{
	if (valueString == "auto") {
		value = FrameSkip(true, value.getFrameSkip());
	} else {
		int tmp = strtol(valueString.c_str(), NULL, 0);
		if ((0 <= tmp) && (tmp <= 100)) {
			value = FrameSkip(false, tmp);
		} else {
			throw CommandException("Not a valid value");
		}
	}
}

// TODO: Find out if there is a cleaner way to instantiate this method.
void FrameSkipSetting::setValue(const FrameSkip &newValue) {
	Setting<FrameSkip>::setValue(newValue);
}
