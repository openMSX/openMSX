#ifndef TCLCALLBACK_HH
#define TCLCALLBACK_HH

#include "TclObject.hh"
#include "string_ref.hh"
#include <memory>

namespace openmsx {

class CommandController;
class StringSetting;

class TclCallback
{
public:
	TclCallback(CommandController& controller,
	            string_ref name,
	            string_ref description,
	            bool useCliComm = true,
	            bool save = true);
	explicit TclCallback(StringSetting& setting);
	~TclCallback();

	TclObject execute();
	TclObject execute(int arg1);
	TclObject execute(int arg1, int arg2);
	TclObject execute(int arg1, string_ref arg2);
	TclObject execute(string_ref arg1, string_ref arg2);

	TclObject getValue() const;
	StringSetting& getSetting() const { return callbackSetting; }

private:
	TclObject executeCommon(TclObject& command);

	std::unique_ptr<StringSetting> callbackSetting2; // can be nullptr
	StringSetting& callbackSetting;
	const bool useCliComm;
};

} // namespace openmsx

#endif
