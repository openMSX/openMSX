// $Id$

#include "Pluggable.hh"
#include "PlugException.hh"
#include "Connector.hh"
#include "unreachable.hh"
#include <cassert>

using std::string;

namespace openmsx {

Pluggable::Pluggable()
{
	setConnector(nullptr);
}

Pluggable::~Pluggable()
{
}

const string& Pluggable::getName() const
{
	static const string name("--empty--");
	return name;
}

void Pluggable::plug(Connector& newConnector, EmuTime::param time)
{
	assert(getClass() == newConnector.getClass());

	if (connector) {
		throw PlugException(getName() + " already plugged in " +
		                    connector->getName() + '.');
	}
	plugHelper(newConnector, time);
	setConnector(&newConnector);
}

void Pluggable::unplug(EmuTime::param time)
{
	try {
		unplugHelper(time);
	} catch (MSXException&) {
		UNREACHABLE;
	}
	setConnector(nullptr);
}

Connector* Pluggable::getConnector() const
{
	return connector;
}

void Pluggable::setConnector(Connector* conn)
{
	connector = conn;
}

} // namespace openmsx
