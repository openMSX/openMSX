#include "JoyTap.hh"
#include "JoystickPort.hh"
#include "PluggingController.hh"
#include "enumerate.hh"
#include "serialize.hh"
#include "strCat.hh"
#include <memory>

namespace openmsx {

using std::string;

JoyTap::JoyTap(PluggingController& pluggingController_, string name_)
	: pluggingController(pluggingController_)
	, name(std::move(name_))
{
}

JoyTap::~JoyTap() = default;

void JoyTap::createPorts(static_string_view description) {
	for (auto [i, slave] : enumerate(slaves)) {
		slave = std::make_unique<JoystickPort>(
			pluggingController,
			strCat(name, "_port_", char('1' + i)),
			description);
	}
}

std::string_view JoyTap::getDescription() const
{
	return "MSX Joy Tap device";
}

std::string_view JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	createPorts("Joy Tap port");
}

void JoyTap::unplugHelper(EmuTime::param time)
{
	for (auto& s : slaves) {
		s->unplug(time);
		s.reset();
	}
}

byte JoyTap::read(EmuTime::param time)
{
	byte value = 255;
	for (auto& s : slaves) {
		value &= s->read(time);
	}
	return value;
}

void JoyTap::write(byte value, EmuTime::param time)
{
	for (auto& s : slaves) {
		s->write(value, time);
	}
}

template<typename Archive>
void JoyTap::serialize(Archive& ar, unsigned /*version*/)
{
	// saving only happens when plugged in
	if constexpr (!ar.IS_LOADER) assert(isPluggedIn());
	// restore plugged state when loading
	if constexpr (ar.IS_LOADER) {
		plugHelper(*getConnector(), pluggingController.getCurrentTime());
	}

	char tag[6] = { 'p', 'o', 'r', 't', 'X', 0 };
	for (auto [i, slave] : enumerate(slaves)) {
		tag[4] = char('0' + i);
		ar.serialize(tag, *slave);
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyTap, "JoyTap");

} // namespace openmsx
