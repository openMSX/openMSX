// $Id$

#ifndef IDEDEVICEFACTORY_HH
#define IDEDEVICEFACTORY_HH

namespace openmsx {

class IDEDevice;
class MSXMotherBoard;
class XMLElement;
class EmuTime;

class IDEDeviceFactory
{
public:
	static IDEDevice* create(MSXMotherBoard& motherBoard,
	                         const XMLElement& config,
	                         const EmuTime& time);
};

} // namespace openmsx

#endif
