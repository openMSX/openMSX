// $Id$

#include "Schedulable.hh"
#include "MSXDevice.hh"


#ifdef DEBUG

const std::string &Schedulable::getName()
{
	MSXDevice *dev = dynamic_cast<MSXDevice*>(this);
	if (dev) {
		return dev->getName();
	} else {
		return defaultName;
	}
}
const std::string Schedulable::defaultName = "[Schedulable, no name]";

#endif


