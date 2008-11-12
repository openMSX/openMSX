// $Id$

#include "JoyTap.hh"
#include "JoystickPort.hh"
#include "serialize.hh"

namespace openmsx {

JoyTap::JoyTap(PluggingController& pluggingController,
               const std::string& name_)
	: name(name_)
{
	for (int i = 0; i < 4; ++i) {
		slaves[i].reset(new JoystickPort(
			pluggingController,
			getName() + "_port_" + char('1' + i)));
	}
}

JoyTap::~JoyTap()
{
}

const std::string& JoyTap::getDescription() const
{
	static const std::string desc("MSX JoyTap device.");
	return desc;
}

const std::string& JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	// TODO move create of the joystickports to here???
}

void JoyTap::unplugHelper(EmuTime::param /*time*/)
{
	// TODO move destruction of the joystickports to here???
}

byte JoyTap::read(EmuTime::param time)
{
	byte value = 255;
	for (int i = 0; i < 4; ++i) {
		value &= slaves[i]->read(time);
	}
	return value;
}

void JoyTap::write(byte value, EmuTime::param time)
{
	for (int i = 0; i < 4; ++i) {
		slaves[i]->write(value, time);
	}
}

template<typename Archive>
void JoyTap::serialize(Archive& ar, unsigned /*version*/)
{
	for (int i = 0; i < 4; ++i) {
		std::string tag = std::string("port") + char('0' + i);
		ar.serialize(tag.c_str(), *slaves[i]);
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyTap, "JoyTap");

} // namespace openmsx
