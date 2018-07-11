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
	             string_view name, string_view description,
	             double initialValue, double minValue, double maxValue);

	string_view getTypeString() const override;
	void additionalInfo(TclObject& result) const override;

	double getDouble() const { return getValue().getDouble(getInterpreter()); }
	void setDouble (double d);

private:
	const double minValue;
	const double maxValue;
};

} // namespace openmsx

#endif
