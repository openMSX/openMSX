// $Id$

#ifndef IDEDEVICEFACTORY_HH
#define IDEDEVICEFACTORY_HH

namespace openmsx {

class IDEDevice;
class EventDistributor;
class XMLElement;
class EmuTime;

class IDEDeviceFactory
{
public:
	static IDEDevice* create(EventDistributor& eventDistributor,
                         const XMLElement& config, const EmuTime& time);
};

} // namespace openmsx

#endif
