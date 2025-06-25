/*
 * Password Cartridge
 *
 * Access: write 0x00 to I/O port 0x7e
 *         provide 0xaa, <char1>, <char2>, continuous 0xff sequence reading I/O port 0x7e
 *         write any non-zero to I/O port 0x7e
 *         provide 0xff at all time reading I/O port 0x7e
 */

#include "PasswordCart.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <algorithm>

namespace openmsx {

PasswordCart::PasswordCart(const DeviceConfig& config)
	: MSXDevice(config)
	, password(narrow_cast<uint16_t>(config.getChildDataAsInt("password", 0)))
{
	reset(EmuTime::dummy());
}

void PasswordCart::reset(EmuTime /*time*/)
{
	pointer = 3;
}

void PasswordCart::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	pointer = (value == 0) ? 0 : 3;
}

byte PasswordCart::readIO(uint16_t port, EmuTime time)
{
	byte result = peekIO(port, time);
	pointer = byte(std::min(3, pointer + 1));
	return result;
}

byte PasswordCart::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	switch (pointer) {
	case 0:
		return 0xAA;
	case 1:
		return narrow_cast<byte>(password >> 8);
	case 2:
		return narrow_cast<byte>(password & 0xFF);
	default:
		return 0xFF;
	}
}

template<typename Archive>
void PasswordCart::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("pointer", pointer);
}
INSTANTIATE_SERIALIZE_METHODS(PasswordCart);
REGISTER_MSXDEVICE(PasswordCart, "PasswordCart");

} // namespace openmsx
