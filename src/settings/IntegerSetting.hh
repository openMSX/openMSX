// $Id$

#ifndef __INTEGERSETTING_HH__
#define __INTEGERSETTING_HH__

#include "Setting.hh"
#include "NonInheritable.hh"

namespace openmsx {

/** A Setting with an integer value.
  */
class IntegerSetting: public Setting<int>, NON_INHERITABLE(IntegerSetting)
{
public:
	IntegerSetting(const string& name, const string& description,
	               int initialValue, int minValue, int maxValue);
	virtual ~IntegerSetting();

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(int minValue, int maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString)
		throw(CommandException);
	virtual void setValue(const int& newValue);

protected:
	int minValue;
	int maxValue;
};

} // namespace openmsx

#endif //__INTEGERSETTING_HH__
