#ifndef SONYHBIV1_HH
#define SONYHBIV1_HH

#include "MSXDevice.hh"
#include "Rom.hh"

#include "EmuTime.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class VDP;

/** Sony HBI-V1 video digitizer cartridge.
  *
  * The cartridge has a 32kB ROM (BASIC CALL extensions, 0x4000-0xBFFF), a
  * 256x212 byte "digitizer VRAM" of its own and a few registers, all memory
  * mapped in the top of page 1:
  *
  *   0x7E00-0x7EFF  read window on the digitizer VRAM, auto-incrementing
  *                  (256 bytes per line, 212 lines)
  *   0x7FFC  write: command (bits 1:0: 0 = stop, 1 = digitize now,
  *                           2 = digitize after 'delay' frames); resets the
  *                           read pointer
  *           read:  bit 7 = busy, bits 1:0 = last command
  *   0x7FFD  write: start block (bits 3:2 = Y, bits 1:0 = X)
  *           read:  bit 7 = field parity, bit 4 = no video signal,
  *                  bits 3:0 = start block
  *   0x7FFE  write/read: bits 7:6 = mode (0 = YJK 5-bit Y, 1 = YJK 4-bit Y,
  *                       2 = RGB 3-3-2, 3 = clear), bits 5:3 = block size Y,
  *                       bits 2:0 = block size X
  *   0x7FFF  write/read: delay in frames for command 2
  *
  * The register behaviour follows the one used by other emulators for this
  * cartridge; it has not been verified on real hardware. The video comes from
  * the external video source of the VDP (see the external video panel).
  */
class SonyHBIV1 final : public MSXDevice
{
public:
	explicit SonyHBIV1(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] VDP* getVDP() const;
	[[nodiscard]] EmuDuration frameDuration() const;
	void update(EmuTime time);
	void digitize();
	void digitizeBlock(const std::array<uint8_t, 256 * 212 * 3>& img,
	                   unsigned vramX, unsigned vramY,
	                   unsigned width, unsigned height,
	                   unsigned destX, unsigned destY,
	                   unsigned srcX, unsigned srcY);
	[[nodiscard]] byte readRegister(unsigned reg, EmuTime time, bool sideEffects);

private:
	Rom rom;
	std::array<uint8_t, 256 * 256> vram;

	byte command = 0;
	byte startBlockX = 0;
	byte startBlockY = 0;
	byte blockSizeX = 0;
	byte blockSizeY = 0;
	byte mode = 0;
	byte delay = 0;
	byte vramOffset = 0;
	byte vramLine = 0;
	byte status = 0; // bits that toggle on every status read

	bool pending = false;
	EmuTime pendingTime = EmuTime::infinity();
	EmuTime busyUntil = EmuTime::zero();
};

} // namespace openmsx

#endif
