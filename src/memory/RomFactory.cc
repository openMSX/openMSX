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
#include "RomMSXWrite.hh"
#include "RomPadial8kB.hh"
#include "RomPadial16kB.hh"
#include "RomSuperLodeRunner.hh"
#include "RomSuperSwangi.hh"
#include "RomMitsubishiMLTS2.hh"
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
#include "MegaFlashRomSCCPlusSD.hh"
#include "RomDooly.hh"
#include "RomMSXtra.hh"
#include "RomMultiRom.hh"
#include "Rom.hh"
#include "Reactor.hh"
#include "RomDatabase.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "memory.hh"

using std::unique_ptr;
using std::move;
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

	if (size < 0x10000) {
		if ((size <= 0x4000) &&
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
	} else if (size == 0x10000 && !((data[0] == 'A') && (data[1] == 'B'))) {
		// 64 kB ROMs can be plain or memory mapped...
		// check here for plain, if not, try the auto detection
		// (thanks for the hint, hap)
		return ROM_MIRRORED;
	} else {
		//  GameCartridges do their bankswitching by using the Z80
		//  instruction ld(nn),a in the middle of program code. The
		//  adress nn depends upon the GameCartridge mappertype used.
		//  To guess which mapper it is, we will look how much writes
		//  with this instruction to the mapper-registers-addresses
		//  occur.

		unsigned typeGuess[ROM_END_OF_UNORDERED_LIST] = {}; // 0-initialized
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
		RomType type = ROM_GENERIC_8KB;
		for (int i = 0; i < ROM_END_OF_UNORDERED_LIST; ++i) {
			// debug: fprintf(stderr, "%d: %d\n", i, typeGuess[i]);
			if (typeGuess[i] && (typeGuess[i] >= typeGuess[type])) {
				type = static_cast<RomType>(i);
			}
		}
		// in case of doubt we go for type ROM_GENERIC_8KB
		// in case of even type ROM_ASCII16 and ROM_ASCII8 we would
		// prefer ROM_ASCII16 but we would still prefer ROM_GENERIC_8KB
		// above ROM_ASCII8 or ROM_ASCII16
		if ((type == ROM_ASCII16) &&
		    (typeGuess[ROM_GENERIC_8KB] == typeGuess[ROM_ASCII16])) {
			type = ROM_GENERIC_8KB;
		}
		return type;
	}
}

unique_ptr<MSXDevice> create(const DeviceConfig& config)
{
	Rom rom(config.getAttribute("id"), "rom", config);

	// Get specified mapper type from the config.
	RomType type;
	// if no type is mentioned, we assume 'mirrored' which works for most
	// plain ROMs...
	string_ref typestr = config.getChildData("mappertype", "Mirrored");
	if (typestr == "auto") {
		// Guess mapper type, if it was not in DB.
		const RomInfo* romInfo = config.getReactor().getSoftwareDatabase().fetchRomInfo(rom.getOriginalSHA1());
		if (!romInfo) {
			type = guessRomType(rom);
		} else {
			type = romInfo->getRomType();
		}
	} else {
		// Use mapper type from config, even if this overrides DB.
		type = RomInfo::nameToRomType(typestr);
	}

	unique_ptr<MSXRom> result;
	switch (type) {
	case ROM_MIRRORED:
		result = make_unique<RomPlain>(
			config, move(rom), RomPlain::MIRRORED);
		break;
	case ROM_NORMAL:
		result = make_unique<RomPlain>(
			config, move(rom), RomPlain::NOT_MIRRORED);
		break;
	case ROM_MIRRORED0000:
	case ROM_MIRRORED4000:
	case ROM_MIRRORED8000:
	case ROM_MIRROREDC000:
		result = make_unique<RomPlain>(
			config, move(rom), RomPlain::MIRRORED,
			(type & 7) * 0x2000);
		break;
	case ROM_NORMAL0000:
	case ROM_NORMAL4000:
	case ROM_NORMAL8000:
	case ROM_NORMALC000:
		result = make_unique<RomPlain>(
			config, move(rom), RomPlain::NOT_MIRRORED,
			(type & 7) * 0x2000);
		break;
	case ROM_PAGE0:
	case ROM_PAGE1:
	case ROM_PAGE01:
	case ROM_PAGE2:
	case ROM_PAGE12:
	case ROM_PAGE012:
	case ROM_PAGE3:
	case ROM_PAGE23:
	case ROM_PAGE123:
	case ROM_PAGE0123:
		result = make_unique<RomPageNN>(config, move(rom), type & 0xF);
		break;
	case ROM_DRAM:
		result = make_unique<RomDRAM>(config, move(rom));
		break;
	case ROM_GENERIC_8KB:
		result = make_unique<RomGeneric8kB>(config, move(rom));
		break;
	case ROM_GENERIC_16KB:
		result = make_unique<RomGeneric16kB>(config, move(rom));
		break;
	case ROM_KONAMI_SCC:
		result = make_unique<RomKonamiSCC>(config, move(rom));
		break;
	case ROM_KONAMI:
		result = make_unique<RomKonami>(config, move(rom));
		break;
	case ROM_KBDMASTER:
		result = make_unique<RomKonamiKeyboardMaster>(config, move(rom));
		break;
	case ROM_ASCII8:
		result = make_unique<RomAscii8kB>(config, move(rom));
		break;
	case ROM_ASCII16:
		result = make_unique<RomAscii16kB>(config, move(rom));
		break;
	case ROM_MSXWRITE:
		result = make_unique<RomMSXWrite>(config, move(rom));
		break;
	case ROM_PADIAL8:
		result = make_unique<RomPadial8kB>(config, move(rom));
		break;
	case ROM_PADIAL16:
		result = make_unique<RomPadial16kB>(config, move(rom));
		break;
	case ROM_SUPERLODERUNNER:
		result = make_unique<RomSuperLodeRunner>(config, move(rom));
		break;
	case ROM_SUPERSWANGI:
		result = make_unique<RomSuperSwangi>(config, move(rom));
		break;
	case ROM_MITSUBISHIMLTS2:
		result = make_unique<RomMitsubishiMLTS2>(config, move(rom));
		break;
	case ROM_MSXDOS2:
		result = make_unique<RomMSXDOS2>(config, move(rom));
		break;
	case ROM_R_TYPE:
		result = make_unique<RomRType>(config, move(rom));
		break;
	case ROM_CROSS_BLAIM:
		result = make_unique<RomCrossBlaim>(config, move(rom));
		break;
	case ROM_HARRY_FOX:
		result = make_unique<RomHarryFox>(config, move(rom));
		break;
	case ROM_ASCII8_8:
		result = make_unique<RomAscii8_8>(
			config, move(rom), RomAscii8_8::ASCII8_8);
		break;
	case ROM_ASCII8_2:
		result = make_unique<RomAscii8_8>(
			config, move(rom), RomAscii8_8::ASCII8_2);
		break;
	case ROM_KOEI_8:
		result = make_unique<RomAscii8_8>(
			config, move(rom), RomAscii8_8::KOEI_8);
		break;
	case ROM_KOEI_32:
		result = make_unique<RomAscii8_8>(
			config, move(rom), RomAscii8_8::KOEI_32);
		break;
	case ROM_WIZARDRY:
		result = make_unique<RomAscii8_8>(
			config, move(rom), RomAscii8_8::WIZARDRY);
		break;
	case ROM_ASCII16_2:
		result = make_unique<RomAscii16_2>(config, move(rom), RomAscii16_2::ASCII16_2);
		break;
	case ROM_ASCII16_8:
		result = make_unique<RomAscii16_2>(config, move(rom), RomAscii16_2::ASCII16_8);
		break;
	case ROM_GAME_MASTER2:
		result = make_unique<RomGameMaster2>(config, move(rom));
		break;
	case ROM_PANASONIC:
		result = make_unique<RomPanasonic>(config, move(rom));
		break;
	case ROM_NATIONAL:
		result = make_unique<RomNational>(config, move(rom));
		break;
	case ROM_MAJUTSUSHI:
		result = make_unique<RomMajutsushi>(config, move(rom));
		break;
	case ROM_SYNTHESIZER:
		result = make_unique<RomSynthesizer>(config, move(rom));
		break;
	case ROM_PLAYBALL:
		result = make_unique<RomPlayBall>(config, move(rom));
		break;
	case ROM_NETTOU_YAKYUU:
		result = make_unique<RomNettouYakyuu>(config, move(rom));
		break;
	case ROM_HALNOTE:
		result = make_unique<RomHalnote>(config, move(rom));
		break;
	case ROM_ZEMINA80IN1:
		result = make_unique<RomZemina80in1>(config, move(rom));
		break;
	case ROM_ZEMINA90IN1:
		result = make_unique<RomZemina90in1>(config, move(rom));
		break;
	case ROM_ZEMINA126IN1:
		result = make_unique<RomZemina126in1>(config, move(rom));
		break;
	case ROM_HOLY_QURAN:
		result = make_unique<RomHolyQuran>(config, move(rom));
		break;
	case ROM_HOLY_QURAN2:
		result = make_unique<RomHolyQuran2>(config, move(rom));
		break;
	case ROM_FSA1FM1:
		result = make_unique<RomFSA1FM1>(config, move(rom));
		break;
	case ROM_FSA1FM2:
		result = make_unique<RomFSA1FM2>(config, move(rom));
		break;
	case ROM_MANBOW2:
	case ROM_MANBOW2_2:
	case ROM_HAMARAJANIGHT:
	case ROM_MEGAFLASHROMSCC:
		result = make_unique<RomManbow2>(config, move(rom), type);
		break;
	case ROM_MATRAINK:
		result = make_unique<RomMatraInk>(config, move(rom));
		break;
	case ROM_ARC:
		result = make_unique<RomArc>(config, move(rom));
		break;
	case ROM_MEGAFLASHROMSCCPLUS:
		result = make_unique<MegaFlashRomSCCPlus>(config, move(rom));
		break;
	case ROM_MEGAFLASHROMSCCPLUSSD:
		result = make_unique<MegaFlashRomSCCPlusSD>(config, move(rom));
		break;
	case ROM_DOOLY:
		result = make_unique<RomDooly>(config, move(rom));
		break;
	case ROM_MSXTRA:
		result = make_unique<RomMSXtra>(config, move(rom));
		break;
	case ROM_MULTIROM:
		result = make_unique<RomMultiRom>(config, move(rom));
		break;
	default:
		throw MSXException("Unknown ROM type");
	}

	// Store actual detected mapper type in config (override the possible
	// 'auto' value). This way we're sure that on savestate/loadstate we're
	// using the same mapper type (for example when the user's rom-database
	// was updated).
	auto& writableConfig = const_cast<XMLElement&>(*config.getXML());
	writableConfig.setChildData("mappertype", RomInfo::romTypeToName(type));

	return move(result);
}

} // namespace RomFactory
} // namespace openmsx
