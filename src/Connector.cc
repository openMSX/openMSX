// $Id$

#include "Connector.hh"
#include "Pluggable.hh"

Connector::Connector()
{
	pluggable = NULL;
}

void Connector::plug(Pluggable *device)
{
	device->plug();
	pluggable = device;
}

void Connector::unplug()
{
	if (pluggable) pluggable->unplug();
	//pluggable = DUMMY_DEVICE;
}
