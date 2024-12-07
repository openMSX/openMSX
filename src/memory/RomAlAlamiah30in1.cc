// Al Alamiah 30-in-1 cartridge series
//
// Wouter's analysis on the schematic that we drew up with the PCB:
// * pin /SLTSL of the MSX cartridge slot is used (not /CS1, /CS2 or
//   /CS12). This means the device reacts to reads on the whole memory
//   region 0x0000..0xffff.
// * pin A15 from the MSX cartridge slot does not seem to be connected.
//   That means memory region 0x0000-0x7ffff is mirrored in range
//   0x8000..0xffff.
// * Only pins A7..A3 (not A2..A0) seem to be used in the decoding of the
//   IO-port number. That means the cartridge will react to port range
//   0..7.
// * The 74LS174 can only store 6 bits (not 8). Only data bits D0..D5 of the
//   MSX cartridge slot are connected to the 74LS174. That means that the upper
//   two bits that are written via an OUT 0-7 operation are ignored.
// * The 74LS174 /RESET input is connected to the /RESET signal of the MSX
//   cartridge slot. That means that on reset, the output pins Q0..Q5 of the
//   74LS174 are set to zero. Thus bank 0 gets selected and (see later) we again
//   allow to select a new bank.
// * The 74LS174 latches the input on a rising clock-edge. That mean a change
//   from 0 to 1 on the CLOCK input pin. See later for how this CLOCK input
//   signal gets calculated
// * The 74LS02 chips offers 4 NOR gates, it seems only 2 of these are used.
// * For one of these two NOR gates both inputs are connected to the same
//   signal (A7 of the MSX cartridge slot). This effectively turns this NOR gate
//   into a NOT gate.
// * The 74LS138 chip is a 3-to-8-decoder. The datasheet(s) I found use a
//   different naming convention for the pin names than what is used in this
//   schematic. So I'm making some assumptions here:
//   * I'm assuming the A0, A1, A2 pins are the address-input pins. These 3
//     pins are interpreted as a value (between 0 and 7) and select one of the
//     8 output pins.
//   * In this schematic only O0 is connected. This output pin becomes active
//     (= low) when the inputs A2,A1,A0 are 0,0,0.
//   * In addition the 74LS138 has several enable pins, I'm assuming those are
//     the E1, E2, E3 pins in the schematic.
//   * The decoder is only 'enabled' when two of these enable-inputs are 0 and
//     the 3rd is 1. Unfortunately in this schematic it's not clear which of
//     E1,E2,E3 plays which role (but I can make an educated guess).
//   * The inputs of the decoder are connected to the A7, A6, A5, A4, A3 and
//     /IORQ signals of the MSX cartridge slot. We want this device to react on
//     "OUT 0-7".
//     Thus all these signals (A7..A3 and /IORQ) must be zero. That's indeed the
//     case in this schematic: A7 gets inverted (via a NOT gate) and routed to
//     the E3 input (so we can assume this is the enable-input that must be 1),
//     and all other inputs must be 0 for the output O0 to become 0 (=active)
//     (in all other cases the output O0 will be 1).
// * The output O0 of the 74LS138 (active on a "OUT 0-7") is routed to the
//   input of a NOR gate.
// * The other input of this NOR gate is the Q5 output of the 74LS174 (this
//   chip remembers the last written value to port 0).
// * A NOR gate outputs a 1 when both inputs are 0 (and 0 in all other cases).
// * This means the output can only be 1 when we're processing a "OUT 0-7"
//   operation (good). And in addition Q5 must (still) be 0.
// * The output of the above NOR gate is routed to the CLOCK input of the
//   74LS174. We should examine when this signal changes from 0 to 1. Lets do
//   this in two steps:
//   * First: assume the Q5 output is 0 (this is the case after a reset).
//     * Then the signal changes from 0 to 1 on an "OUT 0-7" operation. That's
//       indeed what we want.
//     * In other words: on an "OUT 0-7" instruction we latch (=remember) the
//       bits D5..D0 (and ignore D7..D6) that were written in the OUT operation.
//   * Second: assume the Q5 output is 1. This can only happen when a prior
//     write has set this bit to 1.
//     * In this case the output of the NOR gate will always be 0. In other
//       words, it cannot change from 0 to 1 anymore. And then it's impossible
//       to change the latched value in the 74LS174.
//    * The only way to "recover" is via a RESET (this sets the Q5 output back
//      to 0).
//
// Summary:
// * Writing a value 0..31, thus with bit 5 = 0 (and ignore bit 6 and 7) to
//   port 0..7 will select the corresponding bank.
// * Writing a different value 0..31 selects a different bank.
// * Writing a value 32..63, thus bit5 = 1 (ignore bits 6,7) will also select a
//   bank, but in addition this will block any further writes (writes to port
//   0..7 will have no effect).
// * The only way to recover from this is via a RESET. This reset will also
//   select bank 0.
//
// (End analysis by Wouter.)
//
// Also important to note: the /WR pin is not connected, so the mapper doesn't
// know whether we're reading from or writing to it (either I/O or memory)
//
// Note that on real hardware, there seem to be many cases that block 31 of the
// mapper is selected at random. I guess this is because of some noise on the
// bus or something... but that must have been one of the reasons to implement
// this block locking mechanism. Another reason could be protection against the
// software in the blocks writing to ports 0-7. But that should not be very
// common.
//
// Many thanks to tsjakoe for helping to reverse engineer the mapper and
// dumping a ROM to test. And of course to Hashem for sending me the Group B
// cartridge so we could reverse engineer the mapper in the first place.

#include "RomAlAlamiah30in1.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomAlAlamiah30in1::RomAlAlamiah30in1(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
{
	reset(EmuTime::dummy());
	getCPUInterface().register_IO_InOut_range(0x00, 8, this);
}

RomAlAlamiah30in1::~RomAlAlamiah30in1()
{
	getCPUInterface().unregister_IO_InOut_range(0x00, 8, this);
}

void RomAlAlamiah30in1::reset(EmuTime::param time)
{
	mapperLocked = false;
	writeIO(0, 0, time);
}

void RomAlAlamiah30in1::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	if (!mapperLocked) {
		// setRom works with 16kB pages as we're using Rom16kBBlocks,
		// but we deal with 32kB blocks in the ROM image
		byte page = 2 * (value & 0b1'1111);
		setRom(0, page + 0);
		setRom(1, page + 1);
		setRom(2, page + 0);
		setRom(3, page + 1);
	}
	// bit 5 of the value sets the mapper to a locked position
	mapperLocked = mapperLocked || ((value & 0b10'0000) != 0);
}

byte RomAlAlamiah30in1::readIO(word port, EmuTime::param time)
{
	// as the cartridge doesn't look at the WR signal, assume a write
	// of 0xFF (as if there are pull-up resistors on the bus).
	writeIO(port, 0xFF, time);
	return 0xFF;
}

template<typename Archive>
void RomAlAlamiah30in1::serialize(Archive& ar, unsigned /*version*/)
{
        ar.template serializeBase<Rom16kBBlocks>(*this);
        ar.serialize("mapperLocked",  mapperLocked);
}
INSTANTIATE_SERIALIZE_METHODS(RomAlAlamiah30in1);

REGISTER_MSXDEVICE(RomAlAlamiah30in1, "RomAlAlamiah30in1");

} // namespace openmsx
