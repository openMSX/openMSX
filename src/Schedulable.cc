// $Id$

#include "Schedulable.hh"
#include "MSXDevice.hh"


const string &Schedulable::schedName() const
{
	static const string defaultName("[Schedulable, no name]");
	
	const MSXDevice *dev = dynamic_cast<const MSXDevice*>(this);
	if (dev) {
		return dev->getName();
	} else {
		return defaultName;
	}
}

