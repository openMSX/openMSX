// $Id$

#ifndef TCLCALLBACK_HH
#define TCLCALLBACK_HH

#include "noncopyable.hh"
#include "string_ref.hh"
#include <memory>

namespace openmsx {

class CommandController;
class Setting;
class StringSetting;
class TclObject;

class TclCallback : private noncopyable
{
public:
	TclCallback(CommandController& controller,
	            string_ref name,
	            string_ref description,
	            bool useCliComm = true,
	            bool save = true);
	TclCallback(StringSetting& setting);
	~TclCallback();

	void execute();
	void execute(int arg1, int arg2);
	void execute(int arg1, string_ref arg2);
	void execute(string_ref arg1, string_ref arg2);

	std::string getValue() const;
	StringSetting& getSetting() const;

private:
	void executeCommon(TclObject& command);

	std::auto_ptr<StringSetting> callbackSetting2;
	StringSetting& callbackSetting;
	const bool useCliComm;
};

} // namespace openmsx

#endif
