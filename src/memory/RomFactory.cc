// $Id$

#include "RomFactory.hh"
#include "RomTypes.hh"
#include "RomInfo.hh"
#include "RomPageNN.hh"
#include "RomPlain.hh"
#include "RomDRAM.hh"
#include "RomGeneric8kB.hh"
#include "RomGeneric16kB.hh"
#include "RomKonami4.hh"
#include "RomKonami5.hh"
#include "RomAscii8kB.hh"
#include "RomAscii8_8.hh"
#include "RomAscii16kB.hh"
#include "RomHydlide2.hh"
#include "RomRType.hh"
#include "RomCrossBlaim.hh"
#include "RomMSXAudio.hh"
#include "RomHarryFox.hh"
#include "RomPanasonic.hh"
#include "RomNational.hh"
#include "RomMajutsushi.hh"
#include "RomSynthesizer.hh"
#include "RomGameMaster2.hh"
#include "RomHalnote.hh"
#include "RomKorean80in1.hh"
#include "RomKorean90in1.hh"
#include "RomKorean126in1.hh"
#include "RomHolyQuran.hh"
#include "RomFSA1FM.hh"
#include "Rom.hh"
#include "XMLElement.hh"
#include "MSXException.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {

static RomType guessRomType(const Rom& rom)
{
	int size = rom.getSize();
	if (size == 0) {
		return ROM_NORMAL;
	}
	const byte* data = &rom[0];

	if (size <= 0x10000) {
		if (size == 0x10000) {
			// There are some programs convert from tape to
			// 64kB rom cartridge these 'fake'roms are from
			// the ASCII16 type
			return ROM_ASCII16;
		} else if ((size <= 0x4000) &&
		           (data[0] == 'A') && (data[1] == 'B')) {
			word initAddr = data[2] + 256 * data[3];
			word textAddr = data[8] + 256 * data[9];
			if ((textAddr & 0xC000) == 0x8000) {
				if ((initAddr == 0) ||
				    (((initAddr & 0xC000) == 0x8000) &&
				     (data[initAddr & (size - 1)] == 0xC9))) {
					return ROM_PAGE2;
				}
			}
		}
		// not correct for Konami-DAC, but does this really need
		// to be correct for _every_ rom?
		return ROM_MIRRORED;
	} else {
		//  GameCartridges do their bankswitching by using the Z80
		//  instruction ld(nn),a in the middle of program code. The
		//  adress nn depends upon the GameCartridge mappertype used.
		//  To guess which mapper it is, we will look how much writes
		//  with this instruction to the mapper-registers-addresses
		//  occur.

		unsigned int typeGuess[] = {0,0,0,0,0,0};
		for (int i=0; i<size-3; i++) {
			if (data[i] == 0x32) {
				word value = data[i+1]+(data[i+2]<<8);
				switch (value) {
					case 0x5000:
					case 0x9000:
					case 0xb000:
						typeGuess[ROM_KONAMI_SCC]++;
						break;
					case 0x4000:
						typeGuess[ROM_KONAMI]++;
						break;
					case 0x8000:
					case 0xa000:
						typeGuess[ROM_KONAMI]++;
						break;
					case 0x6800:
					case 0x7800:
						typeGuess[ROM_ASCII8]++;
						break;
					case 0x6000:
						typeGuess[ROM_KONAMI]++;
						typeGuess[ROM_ASCII8]++;
						typeGuess[ROM_ASCII16]++;
						break;
					case 0x7000:
						typeGuess[ROM_KONAMI_SCC]++;
						typeGuess[ROM_ASCII8]++;
						typeGuess[ROM_ASCII16]++;
						break;
					case 0x77ff:
						typeGuess[ROM_ASCII16]++;
						break;
				}
			}
		}
		if (typeGuess[ROM_ASCII8]) typeGuess[ROM_ASCII8]--; // -1 -> max_int
		RomType type = ROM_GENERIC_8KB; // 0
		for (int i=ROM_GENERIC_8KB; i <= ROM_ASCII16; i++) {
			if ((typeGuess[i]) && (typeGuess[i]>=typeGuess[type])) {
				type = (RomType)i;
			}
		}
		// in case of doubt we go for type 0
		// in case of even type 5 and 4 we would prefer 5
		// but we would still prefer 0 above 4 or 5
		if ((type == ROM_ASCII16) &&
		    (typeGuess[ROM_GENERIC_8KB] == typeGuess[ROM_ASCII16])) {
			type = ROM_GENERIC_8KB;
		}
		return type;
	}
}

auto_ptr<MSXDevice> RomFactory::create(const XMLElement& config,
                                       const EmuTime& time)
{
	auto_ptr<Rom> rom(new Rom(config.getId(), "rom", config));

	// Get specified mapper type from the config.
	RomType type;
	string typestr = config.getChildData("mappertype", "Mirrored");
	if (typestr == "auto") {
		// Guess mapper type, if it was not in DB.
		type = rom->getInfo().getRomType();
		if (type == ROM_UNKNOWN) {
			type = guessRomType(*rom);
		}
	} else {
		// Use mapper type from config, even if this overrides DB.
		type = RomInfo::nameToRomType(typestr);
	}
	PRT_DEBUG("RomType: " << type);

	switch (type) {
	case ROM_MIRRORED:
		return auto_ptr<MSXDevice>(new RomPlain(config, time, rom,
		                 RomPlain::MIRRORED));
	case ROM_NORMAL:
		return auto_ptr<MSXDevice>(new RomPlain(config, time, rom,
		                 RomPlain::NOT_MIRRORED));
	case ROM_MIRRORED0000:
	case ROM_MIRRORED2000:
	case ROM_MIRRORED4000:
	case ROM_MIRRORED6000:
	case ROM_MIRRORED8000:
	case ROM_MIRROREDA000:
	case ROM_MIRROREDC000:
	case ROM_MIRROREDE000:
		return auto_ptr<MSXDevice>(new RomPlain(config, time, rom,
		                 RomPlain::MIRRORED, (type & 7) * 0x2000));
	case ROM_NORMAL0000:
	case ROM_NORMAL2000:
	case ROM_NORMAL4000:
	case ROM_NORMAL6000:
	case ROM_NORMAL8000:
	case ROM_NORMALA000:
	case ROM_NORMALC000:
	case ROM_NORMALE000:
		return auto_ptr<MSXDevice>(new RomPlain(config, time, rom,
		                 RomPlain::NOT_MIRRORED, (type & 7) * 0x2000));
	case ROM_PAGE0:
	case ROM_PAGE1:
	case ROM_PAGE01:
	case ROM_PAGE2:
	case ROM_PAGE02:
	case ROM_PAGE12:
	case ROM_PAGE012:
	case ROM_PAGE3:
	case ROM_PAGE03:
	case ROM_PAGE13:
	case ROM_PAGE013:
	case ROM_PAGE23:
	case ROM_PAGE023:
	case ROM_PAGE123:
	case ROM_PAGE0123:
		return auto_ptr<MSXDevice>(new RomPageNN(config, time, rom, type & 0xF));
	case ROM_DRAM:
		return auto_ptr<MSXDevice>(new RomDRAM(config, time, rom));
	case ROM_GENERIC_8KB:
		return auto_ptr<MSXDevice>(new RomGeneric8kB(config, time, rom));
	case ROM_GENERIC_16KB:
		return auto_ptr<MSXDevice>(new RomGeneric16kB(config, time, rom));
	case ROM_KONAMI_SCC:
		return auto_ptr<MSXDevice>(new RomKonami5(config, time, rom));
	case ROM_KONAMI:
		return auto_ptr<MSXDevice>(new RomKonami4(config, time, rom));
	case ROM_ASCII8:
		return auto_ptr<MSXDevice>(new RomAscii8kB(config, time, rom));
	case ROM_ASCII16:
		return auto_ptr<MSXDevice>(new RomAscii16kB(config, time, rom));
	case ROM_R_TYPE:
		return auto_ptr<MSXDevice>(new RomRType(config, time, rom));
	case ROM_CROSS_BLAIM:
		return auto_ptr<MSXDevice>(new RomCrossBlaim(config, time, rom));
	case ROM_MSX_AUDIO:
		return auto_ptr<MSXDevice>(new RomMSXAudio(config, time, rom));
	case ROM_HARRY_FOX:
		return auto_ptr<MSXDevice>(new RomHarryFox(config, time, rom));
	case ROM_ASCII8_8:
		return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
				       RomAscii8_8::ASCII8_8));
	case ROM_KOEI_8:
		return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
				       RomAscii8_8::KOEI_8));
	case ROM_KOEI_32:
		return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
				       RomAscii8_8::KOEI_32));
	case ROM_WIZARDRY:
		return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
				       RomAscii8_8::WIZARDRY));
	case ROM_ASCII16_2:
		return auto_ptr<MSXDevice>(new RomHydlide2(config, time, rom));
	case ROM_GAME_MASTER2:
		return auto_ptr<MSXDevice>(new RomGameMaster2(config, time, rom));
	case ROM_PANASONIC:
		return auto_ptr<MSXDevice>(new RomPanasonic(config, time, rom));
	case ROM_NATIONAL:
		return auto_ptr<MSXDevice>(new RomNational(config, time, rom));
	case ROM_MAJUTSUSHI:
		return auto_ptr<MSXDevice>(new RomMajutsushi(config, time, rom));
	case ROM_SYNTHESIZER:
		return auto_ptr<MSXDevice>(new RomSynthesizer(config, time, rom));
	case ROM_HALNOTE:
		return auto_ptr<MSXDevice>(new RomHalnote(config, time, rom));
	case ROM_ZEMINA80IN1:
		return auto_ptr<MSXDevice>(new RomKorean80in1(config, time, rom));
	case ROM_ZEMINA90IN1:
		return auto_ptr<MSXDevice>(new RomKorean90in1(config, time, rom));
	case ROM_ZEMINA126IN1:
		return auto_ptr<MSXDevice>(new RomKorean126in1(config, time, rom));
	case ROM_HOLY_QURAN:
		return auto_ptr<MSXDevice>(new RomHolyQuran(config, time, rom));
	case ROM_FSA1FM1:
		return auto_ptr<MSXDevice>(new RomFSA1FM1(config, time, rom));
	case ROM_FSA1FM2:
		return auto_ptr<MSXDevice>(new RomFSA1FM2(config, time, rom));
	default:
		throw FatalError("Unknown ROM type");
	}
}

} // namespace openmsx

