// $Id$

#include "Connector.hh"
#include "Pluggable.hh"

namespace openmsx {

Connector::Connector(const std::string& name_, std::auto_ptr<Pluggable> dummy_)
	: name(name_), dummy(dummy_)
{
	plugged = dummy.get();
}

Connector::~Connector()
{
}

const std::string& Connector::getName() const
{
	return name;
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
