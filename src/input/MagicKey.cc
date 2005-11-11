// $Id$

#include "MagicKey.hh"

namespace openmsx {

// Pluggable
const std::string& MagicKey::getName() const
{
	static const std::string NAME = "magic-key";
	return NAME;
}

const std::string& MagicKey::getDescription() const
{
	static const std::string DESC =
		"Dongle used by some Japanese games to enable cheat mode";
	return DESC;
}

void MagicKey::plugHelper(Connector& /*connector*/, const EmuTime& /*time*/)
{
}

void MagicKey::unplugHelper(const EmuTime& /*time*/)
{
}


// JoystickDevice
byte MagicKey::read(const EmuTime& /*time*/)
{
	return JOY_BUTTONB | JOY_BUTTONA | JOY_RIGHT | JOY_LEFT;
}

void MagicKey::write(byte /*value*/, const EmuTime& /*time*/)
{
}

} // namespace openmsx
