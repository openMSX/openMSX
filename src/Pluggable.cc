#include "Pluggable.hh"
#include "PlugException.hh"
#include "Connector.hh"
#include "unreachable.hh"
#include <cassert>

namespace openmsx {

Pluggable::Pluggable()
{
	setConnector(nullptr);
}

std::string_view Pluggable::getName() const
{
	return "";
}

void Pluggable::plug(Connector& newConnector, EmuTime::param time)
{
	assert(getClass() == newConnector.getClass());

	if (connector) {
		throw PlugException(getName(), " already plugged in ",
		                    connector->getName(), '.');
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
