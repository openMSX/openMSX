// $Id$

#include "Connector.hh"


namespace openmsx {

Connector::Connector(const string &name, Pluggable *dummy)
	: name(name)
	, dummy(dummy)
{
	pluggable = dummy;
}

Connector::~Connector()
{
	delete dummy;
}

void Connector::plug(Pluggable *device, const EmuTime &time)
	throw(PlugException)
{
	device->plug(this, time);
	pluggable = device; // not executed if plug fails
}

void Connector::unplug(const EmuTime &time)
{
	pluggable->unplug(time);
	pluggable = dummy;
}

} // namespace openmsx
