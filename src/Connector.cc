// $Id$

#include "Connector.hh"
#include "Pluggable.hh"

Connector::Connector()
{
	pluggable = NULL;
}

void Connector::plug(Pluggable *device, const EmuTime &time)
{
	device->plug(time);
	pluggable = device;
}

void Connector::unplug(const EmuTime &time)
{
	if (pluggable) pluggable->unplug(time);
	//pluggable = DUMMY_DEVICE;
}
