// $Id$

#ifndef FRONTSWITCH_HH
#define FRONTSWITCH_HH

#include "DeviceConfig.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class BooleanSetting;
class CommandController;
class CliComm;

class FirmwareSwitch : private noncopyable
{
public:
	FirmwareSwitch(CommandController& commandController,
	               const DeviceConfig& config);
	~FirmwareSwitch();

	bool getStatus() const;

private:
	const DeviceConfig config;
	const std::auto_ptr<BooleanSetting> setting;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
