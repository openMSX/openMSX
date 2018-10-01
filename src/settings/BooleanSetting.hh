#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "Setting.hh"

namespace openmsx {

class BooleanSetting final : public Setting
{
public:
	BooleanSetting(CommandController& commandController,
	               string_view name, string_view description,
	               bool initialValue, SaveSetting save = SAVE);
	string_view getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	bool getBoolean() const { return getValue().getBoolean(getInterpreter()); }
	void setBoolean(bool b) { setValue(TclObject(toString(b))); }

private:
	static string_view toString(bool b) { return b ? "true" : "false"; }
};

} // namespace openmsx

#endif
