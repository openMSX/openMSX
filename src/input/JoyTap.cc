#include "JoyTap.hh"

#include "PluggingController.hh"
#include "serialize.hh"

#include "enumerate.hh"
#include "strCat.hh"

#include <array>

namespace openmsx {

JoyTap::JoyTap(PluggingController& pluggingController_, std::string name_)
	: pluggingController(pluggingController_)
	, name(std::move(name_))
{
}

void JoyTap::createPorts(std::string_view description, EmuTime time)
{
	for (auto [i, slave] : enumerate(slaves)) {
		slave.emplace(
			pluggingController,
			strCat(name, "_port_", char('1' + i)),
			strCat(description, ' ', char('1' + i)));
		slave->write(0, time);
	}
}

zstring_view JoyTap::getDescription() const
{
	return "MSX Joy Tap device";
}

zstring_view JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, EmuTime time)
{
	createPorts("Joy Tap port", time);
}

void JoyTap::unplugHelper(EmuTime time)
{
	for (auto& s : slaves) {
		s->unplug(time);
		s.reset();
	}
}

uint8_t JoyTap::read(EmuTime time)
{
	uint8_t value = 255;
	for (auto& s : slaves) {
		value &= s->read(time);
	}
	return value;
}

void JoyTap::write(uint8_t value, EmuTime time)
{
	for (auto& s : slaves) {
		s->write(value, time);
	}
}

template<typename Archive>
void JoyTap::serialize(Archive& ar, unsigned /*version*/)
{
	// saving only happens when plugged in
	if constexpr (!Archive::IS_LOADER) assert(isPluggedIn());
	// restore plugged state when loading
	if constexpr (Archive::IS_LOADER) {
		plugHelper(*getConnector(), pluggingController.getCurrentTime());
	}

	std::array<char, 6> tag = {'p', 'o', 'r', 't', 'X', 0};
	for (auto [i, slave] : enumerate(slaves)) {
		tag[4] = char('0' + i);
		ar.serialize(tag.data(), *slave);
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyTap, "JoyTap");

} // namespace openmsx
