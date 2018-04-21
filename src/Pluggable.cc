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

const string& Pluggable::getName() const
{
	static const string name;
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

} // namespace openmsx
