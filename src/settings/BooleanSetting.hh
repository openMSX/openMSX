#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "Setting.hh"

namespace openmsx {

class BooleanSetting final : public Setting
{
public:
	BooleanSetting(CommandController& commandController,
	               string_ref name, string_ref description,
	               bool initialValue, SaveSetting save = SAVE);
	string_ref getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	bool getBoolean() const { return getValue().getBoolean(getInterpreter()); }
	void setBoolean(bool b) { setString(toString(b)); }

private:
	static std::string toString(bool b) { return b ? "true" : "false"; }
};

} // namespace openmsx

#endif
