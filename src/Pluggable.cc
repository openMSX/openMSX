// $Id$

#include <cassert>
#include "Pluggable.hh"
#include "Connector.hh"

using std::string;

namespace openmsx {

Pluggable::Pluggable()
	: connector(NULL)
{
}

Pluggable::~Pluggable()
{
}

const string& Pluggable::getName() const
{
	static const string name("--empty--");
	return name;
}

void Pluggable::plug(Connector* newConnector, const EmuTime& time)
{
	assert(getClass() == newConnector->getClass());

	if (connector) {
		throw PlugException(getName() + " already plugged in " +
		                    connector->getName() + ".");
	}
	plugHelper(newConnector, time);
	connector = newConnector;
}

void Pluggable::unplug(const EmuTime& time)
{
	unplugHelper(time);
	connector = NULL;
}

Connector* Pluggable::getConnector() const
{
	return connector;
}

} // namespace openmsx
