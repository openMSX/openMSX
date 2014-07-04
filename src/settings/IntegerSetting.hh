#ifndef INTEGERSETTING_HH
#define INTEGERSETTING_HH

#include "Setting.hh"

namespace openmsx {

/** A Setting with an integer value.
  */
class IntegerSetting : public Setting
{
public:
	IntegerSetting(CommandController& commandController,
	               string_ref name, string_ref description,
	               int initialValue, int minValue, int maxValue);

	virtual string_ref getTypeString() const;
	virtual void additionalInfo(TclObject& result) const;

	int getInt() const { return getValue().getInt(getInterpreter()); }
	void setInt(int i);

private:
	const int minValue;
	const int maxValue;
};

} // namespace openmsx

#endif
