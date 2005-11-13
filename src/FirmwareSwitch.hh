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
	explicit FirmwareSwitch(CommandController& commandController);
	~FirmwareSwitch();

	bool getStatus() const;

private:
	const std::auto_ptr<BooleanSetting> setting;
};

} // namespace openmsx

#endif
