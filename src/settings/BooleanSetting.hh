#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "Setting.hh"

namespace openmsx {

class BooleanSetting final : public Setting
{
public:
	BooleanSetting(CommandController& commandController,
	               std::string_view name, static_string_view description,
	               bool initialValue, SaveSetting save = SAVE);
	[[nodiscard]] std::string_view getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] bool getBoolean() const noexcept {
		return getValue().getBoolean(getInterpreter());
	}
	void setBoolean(bool b) { setValue(TclObject(toString(b))); }

private:
	[[nodiscard]] static std::string_view toString(bool b) {
		 return b ? "true" : "false";
	}
};

} // namespace openmsx

#endif
