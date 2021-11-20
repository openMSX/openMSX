#ifndef INTEGERSETTING_HH
#define INTEGERSETTING_HH

#include "Setting.hh"

namespace openmsx {

/** A Setting with an integer value.
  */
class IntegerSetting final : public Setting
{
public:
	IntegerSetting(CommandController& commandController,
	               std::string_view name, static_string_view description,
	               int initialValue, int minValue, int maxValue,
	               SaveSetting save = SAVE);

	[[nodiscard]] std::string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;

	[[nodiscard]] int getInt() const noexcept { return getValue().getInt(getInterpreter()); }
	void setInt(int i);

private:
	const int minValue;
	const int maxValue;
};

} // namespace openmsx

#endif
