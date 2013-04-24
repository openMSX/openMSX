#include "BooleanSetting.hh"
#include "CommandException.hh"
#include "Completer.hh"
#include "StringOp.hh"

using std::string;
using std::vector;

namespace openmsx {

// class BooleanSettingPolicy

BooleanSettingPolicy::BooleanSettingPolicy()
{
}

string BooleanSettingPolicy::toString(bool value) const
{
	return value ? "true" : "false";
}

bool BooleanSettingPolicy::fromString(const string& str) const
{
	string s = StringOp::toLower(str);
	if        ((s == "true")  || (s == "on") || (s == "yes")) {
		return true;
	} else if ((s == "false") || (s == "off") || (s == "no")) {
		return false;
	} else {
		throw CommandException("not a valid boolean: " + str);
	}
}

string_ref BooleanSettingPolicy::getTypeString() const
{
	return "boolean";
}

void BooleanSettingPolicy::tabCompletion(vector<string>& tokens) const
{
	static const char* const values[] = {
		"true",  "on",  "yes",
		"false", "off", "no",
	};
	Completer::completeString(tokens, values, false); // case insensitive
}


// class BooleanSetting

BooleanSetting::BooleanSetting(
		CommandController& commandController, string_ref name,
		string_ref description, bool initialValue, SaveSetting save)
	: SettingImpl<BooleanSettingPolicy>(
		commandController, name, description, initialValue, save)
{
}

} // namespace openmsx
