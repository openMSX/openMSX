#include "Pluggable.hh"

#include "Connector.hh"
#include "PlugException.hh"

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

void Pluggable::plug(Connector& newConnector, EmuTime time)
{
	assert(getClass() == newConnector.getClass());

	if (connector) {
		throw PlugException(getName(), " already plugged in ",
		                    connector->getName(), '.');
	}
	plugHelper(newConnector, time);
	setConnector(&newConnector);
}

void Pluggable::unplug(EmuTime time)
{
	try {
		unplugHelper(time);
	} catch (MSXException&) {
		UNREACHABLE;
	}
	setConnector(nullptr);
}

} // namespace openmsx
