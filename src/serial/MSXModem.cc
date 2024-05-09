#include "MSXModem.hh"
#include "CacheLine.hh"
#include "serialize.hh"
#include "MSXException.hh"

#include <iostream>

// See also
// https://sourceforge.net/p/msxsyssrc/git/ci/master/tree/modem100/t600/t600-0.mac
// for a disassembly of the code in the ROM. That disassembly source by A.
// Zeilemaker is the technical source for this implementation.
//
// MSX-Modem seems to be specified here:
// http://ngs.no.coocan.jp/doc/wiki.cgi/datapack?page=2%BE%CF+MSX+MODEM

namespace openmsx {

MSXModem::MSXModem(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, sram(getName() + " SRAM", 0x2000, config)
{
	if (rom.size() != 0x10000) {
		throw MSXException("ROM must be exactly 64kB in size!");
	}
	reset(EmuTime::dummy());
}

void MSXModem::reset(EmuTime::param /*time*/)
{
	bank = 0; // TODO: verify
	selectedNCUreg = 0; // TODO: verify
}

void MSXModem::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address == 0x7FC0) {
		//   b2-b0 = ROM segment (4000H-5FFFH)
		//   b3    = off hook
		//   b4    = connect to modem
		//   b5    = speaker
		if (byte newBank = value & 0b111; bank != newBank) {
			bank = newBank;
			invalidateDeviceRCache();
			//std::cerr << "Changed bank to " << int(bank) << "\n";
		}
		 // other bits not implemented
	} else if (address == 0x7F00) {
		// TODO: write NCU data
		// These registers are actually connected to the modem part... but faked here.
		// NCU register 0, write
		// NCU register 1, write
		//   b7 = send break disabled
		//   b3 = disable receiver
		// NCU register 2, write
		// NCU register 3, write
		//   transmitter data
		//std::cerr << "Wrote to NCU reg " << int(selectedNCUreg & 0b11) << ": " << int(value) << "\n";
	} else if (address == 0x7F40) {
		// select NCU register
		selectedNCUreg = value;
		// std::cerr << "Changed selected NCU reg to " << int(selectedNCUreg) << "\n";
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		sram.write(address & 0x1FFF, value);
	}
}

byte* MSXModem::getWriteCacheLine(word address)
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if (address == (0x7F40 & CacheLine::HIGH)) return nullptr;
	if (address == (0x7F00 & CacheLine::HIGH)) return nullptr;
	if ((0x6000 <= address) && (address < 0x8000)) {
		return nullptr;
	}
	return unmappedWrite.data();
}

byte MSXModem::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte MSXModem::peekMem(word address, EmuTime::param /*time*/) const
{
	if (address == 0x7FC0) {
		//  b0    = line polarity
		//  b1    = line polarity
		//  b2    = pulse/tone dail
		return 0; // not implemented
	} else if (address == 0x7F00) {
		// read NCU register data
		// These registers are actually connected to the modem part... but faked here.
		// NCU register 0, read
		//   b0 =
		//   b1 =
		//   b2 =
		//   b3 = break detected
		//   b4 = long loop detected
		//   b6 =
		//   b7 = dial tone detected
		// NCU register 1, read
		//   b3 = empty transmitter
		//   b2 = data in receiver
		//   b1 =
		//   b0 =
		// NCU register 2, read
		//   not used
		// NCU register 3, read
		//   receiver data
		static constexpr std::array<byte, 4> ncuRegsRead = {0x00, 0x08, 0x00, 0x00}; // set "empty transmitter" so software starts up...
		//std::cerr << "Reading from NCU reg " << int(selectedNCUreg & 0b11) << "\n";
		return ncuRegsRead[selectedNCUreg & 0b11];
	} else if ((0x4000 <= address) && (address < 0x6000)) {
		return rom[bank * 0x2000 + (address & 0x1FFF)];
	} else if ((0x6000 <= address) && (address < 0x8000)) {
		return sram[address & 0x1FFF];
	} else {
		return 0xFF;
	}
}

const byte* MSXModem::getReadCacheLine(word address) const
{
	if (address == (0x7FC0 & CacheLine::HIGH)) return nullptr;
	if (address == (0x7F00 & CacheLine::HIGH)) return nullptr;
	if ((0x4000 <= address) && (address < 0x6000)) {
		return &rom[bank * 0x2000 + (address & 0x1FFF)];
	}
	if ((0x6000 <= address) && (address < 0x8000)) {
		return &sram[address & 0x1FFF];
	}
	return unmappedRead.data();
}

template<typename Archive>
void MSXModem::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("SRAM", sram,
		     "bank", bank,
		     "selectedNCUreg", selectedNCUreg);
}
INSTANTIATE_SERIALIZE_METHODS(MSXModem);
REGISTER_MSXDEVICE(MSXModem, "MSXModem");

} // namespace openmsx
