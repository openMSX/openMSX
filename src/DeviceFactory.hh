// $Id$

#ifndef __DEVICEFACTORY_HH__
#define __DEVICEFACTORY_HH__

// forward declaration
class MSXDevice;
class EmuTime;


class DeviceFactory
{
	public:
		static MSXDevice *create(Device *conf, const EmuTime &time);
};

#endif
