// $Id$

#include <sstream>
#include "FrameSkipSetting.hh"
#include "CommandException.hh"
#include "SettingsManager.hh"

using std::ostringstream;

namespace openmsx {

FrameSkipSetting::FrameSkipSetting()
	: Setting<FrameSkip>(
		"frameskip", "set the amount of frameskip",
		FrameSkip(true, 0)
		)
{
	type = "0 - 100 / auto";
	SettingsManager::instance().registerSetting(*this);
}

FrameSkipSetting::~FrameSkipSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
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

void FrameSkipSetting::setValueString(const string &valueString)
	throw(CommandException)
{
	if (valueString == "auto") {
		setValue(FrameSkip(true, getValue().getFrameSkip()));
	} else {
		char* endptr;
		int tmp = strtol(valueString.c_str(), &endptr, 0);
		if (*endptr != 0) {
			throw CommandException("Not a valid integer");
		}
		if ((0 <= tmp) && (tmp <= 100)) {
			setValue(FrameSkip(false, tmp));
		} else {
			throw CommandException("Out of range");
		}
	}
}

// TODO: Find out if there is a cleaner way to instantiate this method.
void FrameSkipSetting::setValue(const FrameSkip &newValue)
{
	Setting<FrameSkip>::setValue(newValue);
}

} // namespace openmsx
