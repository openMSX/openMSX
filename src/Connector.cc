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
	if (!ar.isLoader()) {
		std::string plugName;
		if (plugged != dummy.get()) {
			plugName = plugged->getName();
		}
		ar.serialize("plugName", plugName);
		if (!plugName.empty()) {
			ar.beginSection();
			ar.serializePolymorphic("pluggable", *plugged);
			ar.endSection();
		}
	} else {
		std::string plugName;
		ar.serialize("plugName", plugName);
		if (plugName.empty()) {
			// was not plugged in
			plugged = dummy.get();
		} else if (Pluggable* pluggable =
			       pluggingController.findPluggable(plugName)) {
			plugged = pluggable;
			// set connector before loading the pluggable so that
			// the pluggable can test whether it was connected
			pluggable->setConnector(this);
			ar.skipSection(false);
			ar.serializePolymorphic("pluggable", *plugged);
		} else {
			// was plugged, but we don't have that pluggable anymore
			// TODO print warning
			ar.skipSection(true);
			plugged = dummy.get();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Connector);

} // namespace openmsx
