// $Id$

#include <sstream>
#include "FrameSkipSetting.hh"


FrameSkipSetting::FrameSkipSetting()
	: Setting("frameskip", "set the amount of frameskip")
{
	type = "0 - 100 / auto";

	autoFrameSkip = false;
	frameSkip = 0;
}

std::string FrameSkipSetting::getValueString() const
{
	if (autoFrameSkip) {
		return "auto";
	} else {
		std::ostringstream out;
		out << frameSkip;
		return out.str();
	}
}

void FrameSkipSetting::setValueString(
	const std::string &valueString)
{
	if (valueString == "auto") {
		autoFrameSkip = true;
	} else {
		int tmp = strtol(valueString.c_str(), NULL, 0);
		if ((0 <= tmp) && (tmp <= 100)) {
			autoFrameSkip = false;
			frameSkip = tmp;
		} else {
			throw CommandException("Not a valid value");
		}
	}
}
                                                      
