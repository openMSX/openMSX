// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

namespace openmsx {

class MSXDevice;
class EmuTime;


class DeviceFactory {
public:
	static MSXDevice *create(Device *conf, const EmuTime &time);
};

} // namespace openmsx

#endif
