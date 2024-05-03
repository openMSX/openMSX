// Mapper for "Hai no Majutsushi" from Konami.
// It's a Konami mapper with a DAC.

#include "RomMajutsushi.hh"
#include "serialize.hh"

namespace openmsx {

RomMajutsushi::RomMajutsushi(const DeviceConfig& config, Rom&& rom_)
	: RomKonami(config, std::move(rom_))
	, dac("Majutsushi-DAC", "Hai no Majutsushi's DAC", config)
{
}

void RomMajutsushi::reset(EmuTime::param time)
{
	RomKonami::reset(time);
	dac.reset(time);
}

void RomMajutsushi::writeMem(word address, byte value, EmuTime::param time)
{
	if (0x5000 <= address && address < 0x6000) {
		dac.writeDAC(value, time);
	} else {
		RomKonami::writeMem(address, value, time);
	}
}

byte* RomMajutsushi::getWriteCacheLine(word address)
{
	return (0x5000 <= address && address < 0x6000)
		? nullptr : RomKonami::getWriteCacheLine(address);
}

template<typename Archive>
void RomMajutsushi::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<RomKonami>(*this);
	ar.serialize("DAC", dac);
}
INSTANTIATE_SERIALIZE_METHODS(RomMajutsushi);
REGISTER_MSXDEVICE(RomMajutsushi, "RomMajutsushi");

} // namespace openmsx
