// $Id$

#include "Connector.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "serialize.hh"

namespace openmsx {

Connector::Connector(PluggingController& pluggingController_,
                     const std::string& name_, std::auto_ptr<Pluggable> dummy_)
	: pluggingController(pluggingController_)
	, name(name_), dummy(dummy_)
{
	plugged = dummy.get();
	pluggingController.registerConnector(*this);
}

Connector::~Connector()
{
	pluggingController.unregisterConnector(*this);
}

const std::string& Connector::getName() const
{
	return name;
}

void Connector::plug(Pluggable& device, const EmuTime& time)
{
	device.plug(*this, time);
	plugged = &device; // not executed if plug fails
}

void Connector::unplug(const EmuTime& time)
{
	plugged->unplug(time);
	plugged = dummy.get();
}

Pluggable& Connector::getPlugged() const
{
	return *plugged;
}

template<typename Archive>
void Connector::serialize(Archive& ar, unsigned /*version*/)
{
	std::string plugName;
	if (!ar.isLoader()) {
		if (plugged != dummy.get()) {
			plugName = plugged->getName();
		}
	}
	ar.serialize("pluggable", plugName);
	if (ar.isLoader()) {
		if (Pluggable* pluggable = pluggingController.findPluggable(plugName)) {
			plugged = pluggable;
		} else {
			// TODO print warning
			plugged = dummy.get();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Connector);

} // namespace openmsx
