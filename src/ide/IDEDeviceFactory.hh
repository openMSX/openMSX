// $Id$

#ifndef IDEDEVICEFACTORY_HH
#define IDEDEVICEFACTORY_HH

namespace openmsx {

class IDEDevice;
class XMLElement;
class EmuTime;

class IDEDeviceFactory
{
public:
	static IDEDevice* create(const XMLElement& config, const EmuTime& time);
};

} // namespace openmsx

#endif
