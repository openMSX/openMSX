// $Id$

#ifndef __STRINGSETTING_HH__
#define __STRINGSETTING_HH__

#include "Setting.hh"


namespace openmsx {

/** A Setting with a string value.
  */
class StringSetting : public Setting<string>
{
public:
	StringSetting(const string &name, const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);

};

} // namespace openmsx

#endif //__STRINGSETTING_HH__
