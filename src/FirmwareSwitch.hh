// $Id$

#ifndef FRONTSWITCH_HH
#define FRONTSWITCH_HH

#include <memory>
#include <string>

namespace openmsx {

class BooleanSetting;
class CommandController;
class XMLElement;
class CliComm;

class FirmwareSwitch
{
public:
	FirmwareSwitch(CommandController& commandController, 
	                                      const XMLElement& config);
	~FirmwareSwitch();

	bool getStatus() const;

private:
	const std::auto_ptr<BooleanSetting> setting;
	const XMLElement& config;
	CliComm& cliComm;

};

} // namespace openmsx

#endif
