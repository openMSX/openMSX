// $Id$

#include "Pluggable.hh"
#include "PlugException.hh"
#include "Connector.hh"
#include <cassert>

using std::string;

namespace openmsx {

Pluggable::Pluggable()
{
	setConnector(NULL);
}

Pluggable::~Pluggable()
{
}

const string& Pluggable::getName() const
{
	static const string name("--empty--");
	return name;
}

void Pluggable::plug(Connector& newConnector, const EmuTime& time)
{
	assert(getClass() == newConnector.getClass());

	if (connector) {
		throw PlugException(getName() + " already plugged in " +
		                    connector->getName() + ".");
	}
	plugHelper(newConnector, time);
	setConnector(&newConnector);
}

void Pluggable::unplug(const EmuTime& time)
{
	unplugHelper(time);
	setConnector(NULL);
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
