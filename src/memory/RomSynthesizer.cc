/* On Sat, 3 Apr 2004, Manuel Pazos wrote:
 *
 * As you know, the cartridge has an 8bit D/A, accessed through
 * memory-mapped at address #4000 by the program. openMSX also uses that
 * address to access it. But examining the cartridge board I found that the
 * address is decoded by a LS138 in this way:
 *
 *    /WR = L
 *    A15 = L
 *    A4 = L
 *    /MERQ = L
 *    /SLT = L
 *    A14 = H
 *
 * So any value equal to %01xxxxxxxxx0xxxx should work (i.e.: #4000, #4020,
 * #7C00, etc.)
*/

#include "RomSynthesizer.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

RomSynthesizer::RomSynthesizer(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
	, dac("Synthesizer-DAC", "Konami Synthesizer's DAC", config)
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 1);
	setUnmapped(3);

	reset(getCurrentTime());
}

void RomSynthesizer::reset(EmuTime::param time)
{
	dac.reset(time);
}

void RomSynthesizer::writeMem(word address, byte value, EmuTime::param time)
{
	if ((address & 0xC010) == 0x4000) {
		dac.writeDAC(value, time);
	}
}

byte* RomSynthesizer::getWriteCacheLine(word address)
{
	if ((address & 0xC010 & CacheLine::HIGH) == 0x4000) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomSynthesizer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("DAC", dac);
}
INSTANTIATE_SERIALIZE_METHODS(RomSynthesizer);
REGISTER_MSXDEVICE(RomSynthesizer, "RomSynthesizer");

} // namespace openmsx
