// $Id$

#ifndef __BOOLEANSETTING_HH__
#define __BOOLEANSETTING_HH__

#include "EnumSetting.hh"
#include "NonInheritable.hh"

namespace openmsx {

/** A Setting with a boolean value.
  */
NON_INHERITABLE_PRE(BooleanSetting)
class BooleanSetting : public EnumSettingBase<bool>,
                       NON_INHERITABLE(BooleanSetting)
{
public:
	BooleanSetting(const string& name, const string& description,
	               bool initialValue = false, XMLElement* node = NULL);
	virtual ~BooleanSetting();

private:
	static const EnumSettingBase<bool>::Map& getMap();
};

} // namespace openmsx

#endif //__BOOLEANSETTING_HH__
