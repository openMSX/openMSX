// $Id$

#ifndef __IDEDEVICEFACTORY_HH__
#define __IDEDEVICEFACTORY_HH__

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
