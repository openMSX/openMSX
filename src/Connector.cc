// $Id$

#include "Connector.hh"

namespace openmsx {

Connector::Connector(const string& name_, auto_ptr<Pluggable> dummy_)
	: name(name_), dummy(dummy_)
{
	plugged = dummy.get();
}

Connector::~Connector()
{
}

void Connector::plug(Pluggable* device, const EmuTime& time)
{
	device->plug(this, time);
	plugged = device; // not executed if plug fails
}

void Connector::unplug(const EmuTime& time)
{
	plugged->unplug(time);
	plugged = dummy.get();
}

} // namespace openmsx
