// $Id$

#include "RomFactory.hh"
#include "RomTypes.hh"
#include "RomInfo.hh"
#include "RomPageNN.hh"
#include "RomPlain.hh"
#include "RomDRAM.hh"
#include "RomGeneric8kB.hh"
#include "RomGeneric16kB.hh"
#include "RomKonami.hh"
#include "RomKonamiSCC.hh"
#include "RomKonamiKeyboardMaster.hh"
#include "RomAscii8kB.hh"
#include "RomAscii8_8.hh"
#include "RomAscii16kB.hh"
#include "RomPadial8kB.hh"
#include "RomPadial16kB.hh"
#include "RomSuperLodeRunner.hh"
#include "RomMSXDOS2.hh"
#include "RomAscii16_2.hh"
#include "RomRType.hh"
#include "RomCrossBlaim.hh"
#include "RomHarryFox.hh"
#include "RomPanasonic.hh"
#include "RomNational.hh"
#include "RomMajutsushi.hh"
#include "RomSynthesizer.hh"
#include "RomPlayBall.hh"
#include "RomNettouYakyuu.hh"
#include "RomGameMaster2.hh"
#include "RomHalnote.hh"
#include "RomZemina80in1.hh"
#include "RomZemina90in1.hh"
#include "RomZemina126in1.hh"
#include "RomHolyQuran.hh"
#include "RomHolyQuran2.hh"
#include "RomFSA1FM.hh"
#include "RomManbow2.hh"
#include "RomMatraInk.hh"
#include "RomArc.hh"
#include "MegaFlashRomSCCPlus.hh"
#include "Rom.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RomDatabase.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "serialize.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {
namespace RomFactory {

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

		unsigned typeGuess[] = { 0, 0, 0, 0, 0, 0 };
		for (int i = 0; i < size - 3; ++i) {
			if (data[i] == 0x32) {
				word value = data[i + 1] + (data[i + 2] << 8);
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
		for (int i=ROM_GENERIC_8KB; i <= ROM_ASCII16; ++i) {
			if (typeGuess[i] && (typeGuess[i] >= typeGuess[type])) {
				type = static_cast<RomType>(i);
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

auto_ptr<MSXDevice> create(MSXMotherBoard& motherBoard, const XMLElement& config)
{
	auto_ptr<Rom> rom(new Rom(motherBoard, config.getId(), "rom", config));

	// Get specified mapper type from the config.
	RomType type;
	string typestr = config.getChildData("mappertype", "Mirrored");
	if (typestr == "auto") {
		// Guess mapper type, if it was not in DB.
		const RomInfo* romInfo = motherBoard.getReactor().getSoftwareDatabase().fetchRomInfo(rom->getOriginalSHA1());
		if (romInfo == NULL) {
			type = guessRomType(*rom);
		} else {
			type = romInfo->getRomType();
		}
	} else {
		// Use mapper type from config, even if this overrides DB.
		type = RomInfo::nameToRomType(typestr);
	}

	auto_ptr<MSXRom> result;
	switch (type) {
	case ROM_MIRRORED:
		result.reset(new RomPlain(motherBoard, config, rom,
		                          RomPlain::MIRRORED));
		break;
	case ROM_NORMAL:
		result.reset(new RomPlain(motherBoard, config, rom,
		                          RomPlain::NOT_MIRRORED));
		break;
	case ROM_MIRRORED0000:
	case ROM_MIRRORED2000:
	case ROM_MIRRORED4000:
	case ROM_MIRRORED6000:
	case ROM_MIRRORED8000:
	case ROM_MIRROREDA000:
	case ROM_MIRROREDC000:
	case ROM_MIRROREDE000:
		result.reset(new RomPlain(motherBoard, config, rom,
		                     RomPlain::MIRRORED, (type & 7) * 0x2000));
		break;
	case ROM_NORMAL0000:
	case ROM_NORMAL2000:
	case ROM_NORMAL4000:
	case ROM_NORMAL6000:
	case ROM_NORMAL8000:
	case ROM_NORMALA000:
	case ROM_NORMALC000:
	case ROM_NORMALE000:
		result.reset(new RomPlain(motherBoard, config, rom,
		                 RomPlain::NOT_MIRRORED, (type & 7) * 0x2000));
		break;
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
		result.reset(new RomPageNN(motherBoard, config, rom, type & 0xF));
		break;
	case ROM_DRAM:
		result.reset(new RomDRAM(motherBoard, config, rom));
		break;
	case ROM_GENERIC_8KB:
		result.reset(new RomGeneric8kB(motherBoard, config, rom));
		break;
	case ROM_GENERIC_16KB:
		result.reset(new RomGeneric16kB(motherBoard, config, rom));
		break;
	case ROM_KONAMI_SCC:
		result.reset(new RomKonamiSCC(motherBoard, config, rom));
		break;
	case ROM_KONAMI:
		result.reset(new RomKonami(motherBoard, config, rom));
		break;
	case ROM_KBDMASTER:
		result.reset(new RomKonamiKeyboardMaster(
		                   motherBoard, config, rom));
		break;
	case ROM_ASCII8:
		result.reset(new RomAscii8kB(motherBoard, config, rom));
		break;
	case ROM_ASCII16:
		result.reset(new RomAscii16kB(motherBoard, config, rom));
		break;
	case ROM_PADIAL8:
		result.reset(new RomPadial8kB(motherBoard, config, rom));
		break;
	case ROM_PADIAL16:
		result.reset(new RomPadial16kB(motherBoard, config, rom));
		break;
	case ROM_SUPERLODERUNNER:
		result.reset(new RomSuperLodeRunner(motherBoard, config, rom));
		break;
	case ROM_MSXDOS2:
		result.reset(new RomMSXDOS2(motherBoard, config, rom));
		break;
	case ROM_R_TYPE:
		result.reset(new RomRType(motherBoard, config, rom));
		break;
	case ROM_CROSS_BLAIM:
		result.reset(new RomCrossBlaim(motherBoard, config, rom));
		break;
	case ROM_HARRY_FOX:
		result.reset(new RomHarryFox(motherBoard, config, rom));
		break;
	case ROM_ASCII8_8:
		result.reset(new RomAscii8_8(motherBoard, config, rom,
		                             RomAscii8_8::ASCII8_8));
		break;
	case ROM_KOEI_8:
		result.reset(new RomAscii8_8(motherBoard, config, rom,
		                             RomAscii8_8::KOEI_8));
		break;
	case ROM_KOEI_32:
		result.reset(new RomAscii8_8(motherBoard, config, rom,
		                             RomAscii8_8::KOEI_32));
		break;
	case ROM_WIZARDRY:
		result.reset(new RomAscii8_8(motherBoard, config, rom,
		                             RomAscii8_8::WIZARDRY));
		break;
	case ROM_ASCII16_2:
		result.reset(new RomAscii16_2(motherBoard, config, rom));
		break;
	case ROM_GAME_MASTER2:
		result.reset(new RomGameMaster2(motherBoard, config, rom));
		break;
	case ROM_PANASONIC:
		result.reset(new RomPanasonic(motherBoard, config, rom));
		break;
	case ROM_NATIONAL:
		result.reset(new RomNational(motherBoard, config, rom));
		break;
	case ROM_MAJUTSUSHI:
		result.reset(new RomMajutsushi(motherBoard, config, rom));
		break;
	case ROM_SYNTHESIZER:
		result.reset(new RomSynthesizer(motherBoard, config, rom));
		break;
	case ROM_PLAYBALL:
		result.reset(new RomPlayBall(motherBoard, config, rom));
		break;
	case ROM_NETTOU_YAKYUU:
		result.reset(new RomNettouYakyuu(motherBoard, config, rom));
		break;
	case ROM_HALNOTE:
		result.reset(new RomHalnote(motherBoard, config, rom));
		break;
	case ROM_ZEMINA80IN1:
		result.reset(new RomZemina80in1(motherBoard, config, rom));
		break;
	case ROM_ZEMINA90IN1:
		result.reset(new RomZemina90in1(motherBoard, config, rom));
		break;
	case ROM_ZEMINA126IN1:
		result.reset(new RomZemina126in1(motherBoard, config, rom));
		break;
	case ROM_HOLY_QURAN:
		result.reset(new RomHolyQuran(motherBoard, config, rom));
		break;
	case ROM_HOLY_QURAN2:
		result.reset(new RomHolyQuran2(motherBoard, config, rom));
		break;
	case ROM_FSA1FM1:
		result.reset(new RomFSA1FM1(motherBoard, config, rom));
		break;
	case ROM_FSA1FM2:
		result.reset(new RomFSA1FM2(motherBoard, config, rom));
		break;
	case ROM_MANBOW2:
	case ROM_MEGAFLASHROMSCC:
		result.reset(new RomManbow2(
		                   motherBoard, config, rom, type));
		break;
	case ROM_MATRAINK:
		result.reset(new RomMatraInk(motherBoard, config, rom));
		break;
	case ROM_ARC:
		result.reset(new RomArc(motherBoard, config, rom));
		break;
	case ROM_MEGAFLASHROMSCCPLUS:
		result.reset(new MegaFlashRomSCCPlus(motherBoard, config, rom));
		break;
	default:
		throw MSXException("Unknown ROM type");
	}

	// Store actual detected mapper type in config (override the possible
	// 'auto' value). This way we're sure that on savestate/loadstate we're
	// using the same mapper type (for example when the user's rom-database
	// was updated).
	XMLElement& writableConfig = const_cast<XMLElement&>(config);
	writableConfig.setChildData("mappertype", RomInfo::romTypeToName(type));

	return auto_ptr<MSXDevice>(result);
}

} // namespace RomFactory
} // namespace openmsx
