#include "MagicKey.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

// Pluggable
std::string_view MagicKey::getName() const
{
	return "magic-key";
}

std::string_view MagicKey::getDescription() const
{
	return "Dongle used by some Japanese games to enable cheat mode";
}

void MagicKey::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void MagicKey::unplugHelper(EmuTime::param /*time*/)
{
}


// JoystickDevice
byte MagicKey::read(EmuTime::param /*time*/)
{
	return JOY_BUTTONB | JOY_BUTTONA | JOY_RIGHT | JOY_LEFT;
}

void MagicKey::write(byte /*value*/, EmuTime::param /*time*/)
{
}


template<typename Archive>
void MagicKey::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MagicKey);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MagicKey, "MagicKey");

} // namespace openmsx
