#ifndef MSXDEVICESWITCH_HH
#define MSXDEVICESWITCH_HH

#include "MSXDevice.hh"
#include <array>

namespace openmsx {

/**
 * The MSX2 Hardware Specification says ports 0x41-0x4F are Switched I/O ports.
 * Output of a ID value to port 0x40 selects a specific I/O device and connects
 * it to the CPU.
 *
 * It is possible to give numbers between 0 and 255 as a device number.
 * However, when reading the device number, the complement is given, so 0 and
 * 255 are not used. ID numbers between 1 and 127 are manufacturers ID number
 * as in expanded BIOS call. 128 to 254 are device numbers. As a basic rule,
 * those device which are designed specifically for one machine should contain
 * the manufacturers company ID while peripheral device which can be used for
 * all MSX should have device ID number. Also, Z80 CPU has 16 bit address in
 * I/O space so it is recommended to access in 16 bit by decoding the upper 8
 * bit for those ID which might be expanded in the future. Especially for
 * device which are connected with make ID can expand the address space by 256
 * times so it is future proofed.
 *
 * Maker ID
 *    1 ASCII/Microsoft
 *    2 Canon
 *    3 Casio
 *    4 Fujitsu
 *    5 General
 *    6 Hitachi
 *    7 Kyocera
 *    8 Matsushita
 *    9 Mitsubishi
 *   10 NEC
 *   11 Nippon Gakki
 *   12 JVC
 *   13 Philips
 *   14 Pioneer
 *   15 Sanyo
 *   16 Sharp
 *   17 SONY
 *   18 Spectravideo
 *   19 Toshiba
 *   20 Mitsumi
 *
 * Device ID
 *  128 Image Scanner (Matsushita)
 *  247 Kanji 12x12
 *  254 MPS2 (ASCII)
 */

class MSXSwitchedDevice;

class MSXDeviceSwitch final : public MSXDevice
{
public:
	explicit MSXDeviceSwitch(const DeviceConfig& config);
	~MSXDeviceSwitch() override;

	// (un)register methods for devices
	void registerDevice(byte id, MSXSwitchedDevice* device);
	void unregisterDevice(byte id);
	[[nodiscard]] bool hasRegisteredDevices() const { return count != 0; }

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<MSXSwitchedDevice*, 256> devices;
	unsigned count = 0;
	byte selected = 0;
};

} // namespace openmsx

#endif
