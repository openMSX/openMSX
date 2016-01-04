#include "JoyTap.hh"
#include "JoystickPort.hh"
#include "PluggingController.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

using std::string;

JoyTap::JoyTap(PluggingController& pluggingController_,
               const string& name_)
	: pluggingController(pluggingController_)
	, name(name_)
{
}

JoyTap::~JoyTap()
{
}

void JoyTap::createPorts(const string& baseDescription) {
	for (int i = 0; i < 4; ++i) {
		slaves[i] = make_unique<JoystickPort>(
			pluggingController,
			StringOp::Builder() << name << "_port_" << char('1' + i),
			StringOp::Builder() << baseDescription << char('1' + i));
	}
}

string_ref JoyTap::getDescription() const
{
	return "MSX Joy Tap device";
}

const std::string& JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	createPorts("Joy Tap port ");
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
	if (!ar.isLoader()) assert(isPluggedIn());
	// restore plugged state when loading
	if (ar.isLoader()) {
		plugHelper(*getConnector(), pluggingController.getCurrentTime());
	}

	char tag[6] = { 'p', 'o', 'r', 't', 'X', 0 };
	for (int i = 0; i < 4; ++i) {
		tag[4] = char('0' + i);
		ar.serialize(tag, *slaves[i]);
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyTap, "JoyTap");

} // namespace openmsx
