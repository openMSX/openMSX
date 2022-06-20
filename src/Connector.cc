#include "Connector.hh"
#include "PlugException.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "serialize.hh"
#include "CliComm.hh"

namespace openmsx {

Connector::Connector(PluggingController& pluggingController_,
                     std::string name_, std::unique_ptr<Pluggable> dummy_)
	: pluggingController(pluggingController_)
	, name(std::move(name_))
	, dummy(std::move(dummy_))
	, plugged(dummy.get())
{
	pluggingController.registerConnector(*this);
}

Connector::~Connector()
{
	pluggingController.unregisterConnector(*this);
}

void Connector::plug(Pluggable& device, EmuTime::param time)
{
	device.plug(*this, time);
	plugged = &device; // not executed if plug fails
}

void Connector::unplug(EmuTime::param time)
{
	plugged->unplug(time);
	plugged = dummy.get();
}

template<typename Archive>
void Connector::serialize(Archive& ar, unsigned /*version*/)
{
	std::string plugName;
	if constexpr (!Archive::IS_LOADER) {
		if (plugged != dummy.get()) {
			plugName = plugged->getName();
		}
	}
	ar.serialize("plugName", plugName);

	if constexpr (!Archive::IS_LOADER) {
		if (!plugName.empty()) {
			ar.beginSection();
			ar.serializePolymorphic("pluggable", *plugged);
			ar.endSection();
		}
	} else {
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
			try {
				ar.serializePolymorphic("pluggable", *plugged);
			} catch (PlugException& e) {
				pluggingController.getCliComm().printWarning(
					"Pluggable \"", plugName, "\" failed to re-plug: ",
					e.getMessage());
				pluggable->setConnector(nullptr);
				plugged = dummy.get();
			}
		} else {
			// was plugged, but we don't have that pluggable anymore
			pluggingController.getCliComm().printWarning(
				"Pluggable \"", plugName, "\" was plugged in, "
				"but is not available anymore on this system, "
				"so it will be ignored.");
			ar.skipSection(true);
			plugged = dummy.get();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Connector);

} // namespace openmsx
