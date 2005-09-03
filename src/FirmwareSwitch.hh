// $Id$

#ifndef FRONTSWITCH_HH
#define FRONTSWITCH_HH

#include <memory>

namespace openmsx {

class BooleanSetting;
class CommandController;

class FirmwareSwitch
{
public:
	FirmwareSwitch(CommandController& commandController);
	bool getStatus() const;

private:
	const std::auto_ptr<BooleanSetting> setting;
};

} // namespace openmsx

#endif
