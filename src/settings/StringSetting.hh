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
	StringSettingBase(XMLElement& node, const string& description);

public:
	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString);
};

NON_INHERITABLE_PRE(StringSetting)
class StringSetting : public StringSettingBase, NON_INHERITABLE(StringSetting)
{
public:
	StringSetting(const string& name, const string& description,
	              const string& initialValue);
	StringSetting(XMLElement& node, const string& description);
	virtual ~StringSetting();
};

} // namespace openmsx

#endif //__STRINGSETTING_HH__
