// $Id$

#include "Schedulable.hh"
#include "MSXDevice.hh"


const std::string &Schedulable::schedName()
{
	static const std::string defaultName("[Schedulable, no name]");
	
	MSXDevice *dev = dynamic_cast<MSXDevice*>(this);
	if (dev) {
		return dev->getName();
	} else {
		return defaultName;
	}
}

