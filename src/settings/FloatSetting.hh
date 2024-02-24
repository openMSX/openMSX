#ifndef FLOATSETTING_HH
#define FLOATSETTING_HH

#include "Setting.hh"

namespace openmsx {

/** A Setting with a floating point value.
  */
class FloatSetting final : public Setting
{
public:
	FloatSetting(CommandController& commandController,
	             std::string_view name, static_string_view description,
	             double initialValue, double minValue, double maxValue);

	[[nodiscard]] std::string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;

	[[nodiscard]] double getDouble() const noexcept { return getValue().getDouble(getInterpreter()); }
	[[nodiscard]] float  getFloat()  const noexcept { return getValue().getFloat (getInterpreter()); }
	void setDouble(double d);
	void setFloat (float f);

	double getMinValue() const { return minValue; }
	double getMaxValue() const { return maxValue; }

private:
	const double minValue;
	const double maxValue;
};

} // namespace openmsx

#endif
