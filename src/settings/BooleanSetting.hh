// $Id$

#ifndef __BOOLEANSETTING_HH__
#define __BOOLEANSETTING_HH__

#include "EnumSetting.hh"
#include <map>

using std::map;


namespace openmsx {

/** A Setting with a boolean value.
  */
class BooleanSetting : public EnumSetting<bool>
{
public:
	BooleanSetting(
		const string& name, const string& description,
		bool initialValue = false);

private:
	static const map<string, bool>& getMap();
};

} // namespace openmsx

#endif //__BOOLEANSETTING_HH__
