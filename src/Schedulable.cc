// $Id$

#include "Schedulable.hh"
#include "MSXDevice.hh"


const std::string &Schedulable::schedName() const
{
	static const std::string defaultName("[Schedulable, no name]");
	
	const MSXDevice *dev = dynamic_cast<const MSXDevice*>(this);
	if (dev) {
		return dev->getName();
	} else {
		return defaultName;
	}
}

