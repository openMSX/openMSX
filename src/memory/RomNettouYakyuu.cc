// Based on information from hap/enen, published here:
// http://www.msx.org/forumtopicl8552.html
// and his implementation in blueMSX
//
// Jaleco - Moero!! Nettou Yakyuu '88, 256KB ROM, CRC32 68745C64
// NEC D7756C 146 sample player with internal 256Kbit ROM, undumped. Same
// samples as Famicom Moero!! Pro Yakyuu (Black/Red), recording available at
// http://home.planet.nl/~haps/segali_archive/

// 4*8KB pages at $4000-$BFFF, initialized to 0.
// Mapper chip is exactly the same as ASCII8, except for data bit 7:
// a:011ppxxx xxxxxxxx d:sxxbbbbb
// a:address, d:data, p: page, b:bank, x:don't care
// s:if set, page reads/writes redirect to sample player via a 74HC174, eg.
// write $80 to $7800 -> $A000-$BFFF = sample player I/O

// Sample player (datasheet, 690KB one on
// http://www.datasheetarchive.com/search.php?t=0&q=uPD7756 ):
// reads: always $FF (/BUSY not used)
// writes: d:rsxxiiii
// i:I0-I3 (sample number, latched on /ST rising edge when not playing)
// s:/ST (start), r:1=enable continuous rising edges to /ST (ORed with s),
// 0=falling edge to /RESET
// As I'm unable to measure, s and r bits pin setups are guessed from knowing
// the behaviour (examples underneath).

// examples:
// - $C0: nothing
// - $80: "strike, strike, strike, strike, ..."
// - $80, $C0: "strike"
// - $80, short delay, $00/$40: "stri"
// - $80, short delay, $81: "strike, ball, ball, ball, ball, ..."

// note: writes to sample player are ignored if on page $6000-$7FFF, possibly
// due to conflict with mapper chip, reads still return $FF though.

// regarding this implementation: part of the above behaviour is put in the
// sample player. It may be a good idea to move this behaviour to a more
// specialized class some time in the future.

#include "RomNettouYakyuu.hh"
#include "FileOperations.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"

namespace openmsx {

RomNettouYakyuu::RomNettouYakyuu(const DeviceConfig& config, Rom&& rom_)
	: Rom8kBBlocks(config, std::move(rom_))
	, samplePlayer(
		"Nettou Yakyuu-DAC",
		"Jaleco Moero!! Nettou Yakuu '88 DAC", config,
		strCat(FileOperations::stripExtension(rom.getFilename()), '_'),
		16, "nettou_yakyuu/nettou_yakyuu_")
{
	reset(EmuTime::dummy());
}

void RomNettouYakyuu::reset(EmuTime::param /*time*/)
{
	// ASCII8 behaviour
	setUnmapped(0);
	setUnmapped(1);
	for (auto i : xrange(2, 6)) {
		setRom(i, 0);
	}
	setUnmapped(6);
	setUnmapped(7);

	ranges::fill(redirectToSamplePlayer, false);
	samplePlayer.reset();
}

void RomNettouYakyuu::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((address < 0x4000) || (0xC000 <= address)) return;

	// mapper stuff, like ASCII8
	if ((0x6000 <= address) && (address < 0x8000)) {
		// calculate region in switch zone
		byte region = (address >> 11) & 3;
		redirectToSamplePlayer[region] = (value & 0x80) != 0;
		if (redirectToSamplePlayer[region]) {
			setUnmapped(region + 2);
		} else {
			setRom(region + 2, value);
		}
		return;
	}

	// sample player stuff
	if (!redirectToSamplePlayer[(address >> 13) - 2]) {
		// region not redirected to sample player
		return;
	}

	// bit 7==0: reset
	if (!(value & 0x80)) {
		samplePlayer.reset();
		return;
	}

	// bit 6==1: set no retrigger, don't alter playing sample
	if (value & 0x40) {
		samplePlayer.stopRepeat();
		return;
	}

	samplePlayer.repeat(value & 0xF);
}

byte* RomNettouYakyuu::getWriteCacheLine(word /*address*/)
{
	return nullptr;
}

template<typename Archive>
void RomNettouYakyuu::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom8kBBlocks>(*this);
	ar.serialize("SamplePlayer",           samplePlayer,
	             "redirectToSamplePlayer", redirectToSamplePlayer);
}
INSTANTIATE_SERIALIZE_METHODS(RomNettouYakyuu);
REGISTER_MSXDEVICE(RomNettouYakyuu, "RomNettouYakyuu");

} // namespace openmsx
