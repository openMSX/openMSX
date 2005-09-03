// $Id$

#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "EnumSetting.hh"

namespace openmsx {

/** A Setting with a boolean value.
  */
class BooleanSetting : public EnumSetting<bool>
{
public:
	BooleanSetting(CommandController& CommandController,
	               const std::string& name, const std::string& description,
	               bool initialValue, SaveSetting save = SAVE);

private:
	static const EnumSetting<bool>::Map& getMap();
};

} // namespace openmsx

#endif
