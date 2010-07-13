// $Id$

#include "BooleanSetting.hh"
#include "CommandException.hh"
#include "Completer.hh"
#include "StringOp.hh"
#include <set>

using std::string;
using std::vector;
using std::set;

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

string BooleanSettingPolicy::getTypeString() const
{
	return "boolean";
}

void BooleanSettingPolicy::tabCompletion(vector<string>& tokens) const
{
	set<string> values;
	values.insert("true");
	values.insert("on");
	values.insert("yes");
	values.insert("false");
	values.insert("off");
	values.insert("no");
	Completer::completeString(tokens, values, false); // case insensitive
}


// class BooleanSetting

BooleanSetting::BooleanSetting(
		CommandController& commandController, const string& name,
		const string& description, bool initialValue, SaveSetting save)
	: SettingImpl<BooleanSettingPolicy>(
		commandController, name, description, initialValue, save)
{
}

BooleanSetting::BooleanSetting(
		CommandController& commandController, const char* name,
		const char* description, bool initialValue, SaveSetting save)
	: SettingImpl<BooleanSettingPolicy>(
		commandController, name, description, initialValue, save)
{
}

} // namespace openmsx
