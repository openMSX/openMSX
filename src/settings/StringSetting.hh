// $Id$

#ifndef __STRINGSETTING_HH__
#define __STRINGSETTING_HH__

#include "Setting.hh"
#include "NonInheritable.hh"

namespace openmsx {

/** A Setting with a string value.
  */
class StringSettingBase : public Setting<string>
{
protected:
	StringSettingBase(const string& name, const string& description,
	                  const string& initialValue);

public:
	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString)
		throw(CommandException);
};

class StringSetting : public StringSettingBase, NON_INHERITABLE(StringSetting)
{
public:
	StringSetting(const string& name, const string& description,
	              const string& initialValue);
	virtual ~StringSetting();
};

} // namespace openmsx

#endif //__STRINGSETTING_HH__
