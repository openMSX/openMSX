// $Id$

#include "Connector.hh"
#include "Pluggable.hh"


namespace openmsx {

Connector::Connector()
{
	pluggable = NULL;
}

void Connector::plug(Pluggable *device, const EmuTime &time)
{
	pluggable = device;
	device->plug(this, time);
}

void Connector::unplug(const EmuTime &time)
{
	if (pluggable) {
		pluggable->unplug(time);
	}
	//pluggable = DUMMY_DEVICE;
}

Pluggable* Connector::getPlug() const
{
	return pluggable;
}

} // namespace openmsx
