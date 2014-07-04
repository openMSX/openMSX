#ifndef FLOATSETTING_HH
#define FLOATSETTING_HH

#include "Setting.hh"

namespace openmsx {

/** A Setting with a floating point value.
  */
class FloatSetting : public Setting
{
public:
	FloatSetting(CommandController& commandController,
	             string_ref name, string_ref description,
	             double initialValue, double minValue, double maxValue);

	virtual string_ref getTypeString() const;
	virtual void additionalInfo(TclObject& result) const;

	double getDouble() const { return getValue().getDouble(getInterpreter()); }
	void setDouble (double d);

private:
	const double minValue;
	const double maxValue;
};

} // namespace openmsx

#endif
