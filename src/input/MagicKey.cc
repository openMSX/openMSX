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

void MagicKey::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
}

void MagicKey::unplugHelper(EmuTime /*time*/)
{
}


// JoystickDevice
uint8_t MagicKey::read(EmuTime /*time*/)
{
	return JOY_BUTTONB | JOY_BUTTONA | JOY_RIGHT | JOY_LEFT;
}

void MagicKey::write(uint8_t /*value*/, EmuTime /*time*/)
{
}


template<typename Archive>
void MagicKey::serialize(Archive& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(MagicKey);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MagicKey, "MagicKey");

} // namespace openmsx
