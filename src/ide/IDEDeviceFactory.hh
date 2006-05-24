// $Id$

#ifndef IDEDEVICEFACTORY_HH
#define IDEDEVICEFACTORY_HH

namespace openmsx {

class IDEDevice;
class CommandController;
class EventDistributor;
class XMLElement;
class EmuTime;

class IDEDeviceFactory
{
public:
	static IDEDevice* create(
		CommandController& commandController,
		EventDistributor& eventDistributor,
		const XMLElement& config,
		const EmuTime& time
		);
};

} // namespace openmsx

#endif
