//

/*
 * Password Cartridge
 *
 * Access: write 0x00 to I/O port 0x7e
 *         provide 0xaa, <char1>, <char2>, continuous 0xff sequence reading I/O port 0x7e
 *         write any non-zero to I/O port 0x7e
 *	   provide 0xff at all time reading I/O port 0x7e
 */

#include "PasswordCart.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "XMLElement.hh"

namespace openmsx {

PasswordCart::PasswordCart(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
	password = config.getChildDataAsInt("password", 0);
	reset(time);
}

void PasswordCart::reset(const EmuTime& time)
{
	pointer = 3;
}

void PasswordCart::writeIO(word port, byte value, const EmuTime& /*time*/)
{
	pointer = (value == 0) ? 0 : 3;
}

byte PasswordCart::readIO(word port, const EmuTime& /*time*/)
{
	switch (pointer) {
	case 0:
		++pointer;
		return 0xaa;
	case 1:
		++pointer;
		return password >> 8;
	case 2:
		++pointer;
		return password & 0xff;
	case 3:
		return 0xff;
	default:
		return 0xff;
	}
}

} // namespace
