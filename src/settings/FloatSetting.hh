// $Id$

#ifndef __FLOATSETTING_HH__
#define __FLOATSETTING_HH__

#include "Setting.hh"
#include "NonInheritable.hh"

namespace openmsx {

/** A Setting with a floating point value.
  */
NON_INHERITABLE_PRE(FloatSetting)
class FloatSetting: public Setting<double>, NON_INHERITABLE(FloatSetting)
{
public:
	FloatSetting(const string& name, const string& description,
	             double initialValue, double minValue, double maxValue);
	virtual ~FloatSetting();

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(double minValue, double maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString);
	virtual void setValue(const double& newValue);

protected:
	double minValue;
	double maxValue;
};

} // namespace openmsx

#endif //__FLOATSETTING_HH__
