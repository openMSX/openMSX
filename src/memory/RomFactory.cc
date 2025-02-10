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
#include "RomAscii16X.hh"
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
#include "RomNeo8.hh"
#include "RomNeo16.hh"
#include "RomMajutsushi.hh"
#include "RomSynthesizer.hh"
#include "RomPlayBall.hh"
#include "RomNettouYakyuu.hh"
#include "RomGameMaster2.hh"
#include "RomHalnote.hh"
#include "RomZemina25in1.hh"
#include "RomZemina80in1.hh"
#include "RomZemina90in1.hh"
#include "RomZemina126in1.hh"
#include "RomHolyQuran.hh"
#include "RomHolyQuran2.hh"
#include "RomFSA1FM.hh"
#include "RomManbow2.hh"
#include "RomMatraInk.hh"
#include "RomMatraCompilation.hh"
#include "RomArc.hh"
#include "RomAlAlamiah30in1.hh"
#include "RomRetroHard31in1.hh"
#include "ROMHunterMk2.hh"
#include "MegaFlashRomSCCPlus.hh"
#include "ReproCartridgeV1.hh"
#include "ReproCartridgeV2.hh"
#include "KonamiUltimateCollection.hh"
#include "RomDooly.hh"
#include "RomMSXtra.hh"
#include "RomRamFile.hh"
#include "RomColecoMegaCart.hh"
#include "RomMultiRom.hh"
#include "Rom.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "RomDatabase.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "Yamanooto.hh"
#include "MSXException.hh"

#include "enumerate.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <bit>
#include <memory>

namespace openmsx::RomFactory {

using std::make_unique;
using enum RomType;

[[nodiscard]] static RomType guessRomType(const Rom& rom)
{
	auto size = rom.size();
	if (size == 0) {
		return NORMAL;
	}
	//std::span data = rom; // TODO error with clang-13/libc++
	std::span data{std::to_address(rom.begin()), size};

	if (const size_t signatureOffset = 16, signatureSize = 8; size >= (signatureOffset + signatureSize)) {
		auto signature = std::string_view(std::bit_cast<const char*>(data.data()) + signatureOffset, signatureSize);
		if (signature == std::string_view("ASCII16X")) return ASCII16X;
		if (signature == std::string_view("ROM_NEO8")) return NEO8;
		if (signature == std::string_view("ROM_NE16")) return NEO16;
	}
	if (size < 0x10000) {
		if ((size <= 0x4000) &&
		           (data[0] == 'A') && (data[1] == 'B')) {
			auto initAddr = word(data[2] + 256 * data[3]);
			auto textAddr = word(data[8] + 256 * data[9]);
			if ((textAddr & 0xC000) == 0x8000) {
				if ((initAddr == 0) ||
				    (((initAddr & 0xC000) == 0x8000) &&
				     (data[initAddr & (size - 1)] == 0xC9))) {
					return PAGE2;
				}
			}
		}
		// not correct for Konami-DAC, but does this really need
		// to be correct for _every_ rom?
		return MIRRORED;
	} else if (size == 0x10000 && !((data[0] == 'A') && (data[1] == 'B'))) {
		// 64 kB ROMs can be plain or memory mapped...
		// check here for plain, if not, try the auto detection
		// (thanks for the hint, hap)
		return MIRRORED;
	} else {
		//  GameCartridges do their bank switching by using the Z80
		//  instruction ld(nn),a in the middle of program code. The
		//  address nn depends upon the GameCartridge mapper type used.
		//  To guess which mapper it is, we will look how much writes
		//  with this instruction to the mapper-registers-addresses
		//  occur.

		array_with_enum_index<RomType, unsigned> typeGuess = {}; // 0-initialized
		for (auto i : xrange(size - 3)) {
			if (data[i] == 0x32) {
				auto value = word(data[i + 1] + (data[i + 2] << 8));
				switch (value) {
				case 0x5000:
				case 0x9000:
				case 0xb000:
					typeGuess[KONAMI_SCC]++;
					break;
				case 0x4000:
				case 0x8000:
				case 0xa000:
					typeGuess[KONAMI]++;
					break;
				case 0x6800:
				case 0x7800:
					typeGuess[ASCII8]++;
					break;
				case 0x6000:
					typeGuess[KONAMI]++;
					typeGuess[ASCII8]++;
					typeGuess[ASCII16]++;
					break;
				case 0x7000:
					typeGuess[KONAMI_SCC]++;
					typeGuess[ASCII8]++;
					typeGuess[ASCII16]++;
					break;
				case 0x77ff:
					typeGuess[ASCII16]++;
					break;
				}
			}
		}
		if (typeGuess[ASCII8]) typeGuess[ASCII8]--; // -1 -> max_int
		RomType type = GENERIC_8KB;
		for (auto [i, tg] : enumerate(typeGuess)) {
			if (tg && (tg >= typeGuess[type])) {
				type = static_cast<RomType>(i);
			}
		}
		return type;
	}
}

std::unique_ptr<MSXDevice> create(DeviceConfig& config)
{
	Rom rom(std::string(config.getAttributeValue("id")), "rom", config);

	// Get specified mapper type from the config.
	RomType type = [&] {
		// if no type is mentioned, we assume 'mirrored' which works for most
		// plain ROMs...
		std::string_view typeStr = config.getChildData("mappertype", "Mirrored");
		if (typeStr == "auto") {
			// First check whether the (possibly patched) SHA1 is in the DB
			const RomInfo* romInfo = config.getReactor().getSoftwareDatabase().fetchRomInfo(rom.getSHA1());
			// If not found, try the original SHA1 in the DB
			if (!romInfo) {
				romInfo = config.getReactor().getSoftwareDatabase().fetchRomInfo(rom.getOriginalSHA1());
			}
			// If still not found, guess the mapper type
			if (!romInfo) {
				auto machineType = config.getMotherBoard().getMachineType();
				if (machineType == "Coleco") {
					if (rom.size() == one_of(128*1024u, 256*1024u, 512*1024u, 1024*1024u)) {
						return COLECOMEGACART;
					} else {
						return PAGE23;
					}
				} else {
					return guessRomType(rom);
				}
			} else {
				return romInfo->getRomType();
			}
		} else {
			// Use mapper type from config, even if this overrides DB.
			auto t = RomInfo::nameToRomType(typeStr);
			if (t == RomType::UNKNOWN) {
				throw MSXException("Unknown mappertype: ", typeStr);
			}
			return t;
		}
	}();

	// Store actual detected mapper type in config (override the possible
	// 'auto' value). This way we're sure that on savestate/loadstate we're
	// using the same mapper type (for example when the user's rom-database
	// was updated).
	// We do it at this point so that constructors used below can use this
	// information for warning messages etc.
	auto& doc = config.getXMLDocument();
	doc.setChildData(*config.getXML(), "mappertype", RomInfo::romTypeToName(type).data());

	std::unique_ptr<MSXRom> result;
	switch (type) {
	case MIRRORED:
	case MIRRORED0000:
	case MIRRORED4000:
	case MIRRORED8000:
	case MIRROREDC000:
	case NORMAL:
	case NORMAL0000:
	case NORMAL4000:
	case NORMAL8000:
	case NORMALC000:
		result = make_unique<RomPlain>(config, std::move(rom), type);
		break;
	case PAGE0:
	case PAGE1:
	case PAGE01:
	case PAGE2:
	case PAGE12:
	case PAGE012:
	case PAGE3:
	case PAGE23:
	case PAGE123:
	case PAGE0123:
		result = make_unique<RomPageNN>(config, std::move(rom), type);
		break;
	case DRAM:
		result = make_unique<RomDRAM>(config, std::move(rom));
		break;
	case GENERIC_8KB:
		result = make_unique<RomGeneric8kB>(config, std::move(rom));
		break;
	case GENERIC_16KB:
		result = make_unique<RomGeneric16kB>(config, std::move(rom));
		break;
	case KONAMI_SCC:
		result = make_unique<RomKonamiSCC>(config, std::move(rom));
		break;
	case KONAMI:
		result = make_unique<RomKonami>(config, std::move(rom));
		break;
	case KBDMASTER:
		result = make_unique<RomKonamiKeyboardMaster>(config, std::move(rom));
		break;
	case ASCII8:
		result = make_unique<RomAscii8kB>(config, std::move(rom));
		break;
	case ASCII16:
		result = make_unique<RomAscii16kB>(config, std::move(rom));
		break;
	case ASCII16X:
		result = make_unique<RomAscii16X>(config, std::move(rom));
		break;
	case MSXWRITE:
		result = make_unique<RomMSXWrite>(config, std::move(rom));
		break;
	case PADIAL8:
		result = make_unique<RomPadial8kB>(config, std::move(rom));
		break;
	case PADIAL16:
		result = make_unique<RomPadial16kB>(config, std::move(rom));
		break;
	case SUPERLODERUNNER:
		result = make_unique<RomSuperLodeRunner>(config, std::move(rom));
		break;
	case SUPERSWANGI:
		result = make_unique<RomSuperSwangi>(config, std::move(rom));
		break;
	case MITSUBISHIMLTS2:
		result = make_unique<RomMitsubishiMLTS2>(config, std::move(rom));
		break;
	case MSXDOS2:
		result = make_unique<RomMSXDOS2>(config, std::move(rom));
		break;
	case R_TYPE:
		result = make_unique<RomRType>(config, std::move(rom));
		break;
	case CROSS_BLAIM:
		result = make_unique<RomCrossBlaim>(config, std::move(rom));
		break;
	case HARRY_FOX:
		result = make_unique<RomHarryFox>(config, std::move(rom));
		break;
	case ASCII8_8:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::ASCII8_8);
		break;
	case ASCII8_32:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::ASCII8_32);
		break;
	case ASCII8_2:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::ASCII8_2);
		break;
	case KOEI_8:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::KOEI_8);
		break;
	case KOEI_32:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::KOEI_32);
		break;
	case WIZARDRY:
		result = make_unique<RomAscii8_8>(
			config, std::move(rom), RomAscii8_8::SubType::WIZARDRY);
		break;
	case ASCII16_2:
		result = make_unique<RomAscii16_2>(config, std::move(rom), RomAscii16_2::SubType::ASCII16_2);
		break;
	case ASCII16_8:
		result = make_unique<RomAscii16_2>(config, std::move(rom), RomAscii16_2::SubType::ASCII16_8);
		break;
	case GAME_MASTER2:
		result = make_unique<RomGameMaster2>(config, std::move(rom));
		break;
	case PANASONIC:
		result = make_unique<RomPanasonic>(config, std::move(rom));
		break;
	case NATIONAL:
		result = make_unique<RomNational>(config, std::move(rom));
		break;
	case NEO8:
		result = make_unique<RomNeo8>(config, std::move(rom));
		break;
	case NEO16:
		result = make_unique<RomNeo16>(config, std::move(rom));
		break;
	case MAJUTSUSHI:
		result = make_unique<RomMajutsushi>(config, std::move(rom));
		break;
	case SYNTHESIZER:
		result = make_unique<RomSynthesizer>(config, std::move(rom));
		break;
	case PLAYBALL:
		result = make_unique<RomPlayBall>(config, std::move(rom));
		break;
	case NETTOU_YAKYUU:
		result = make_unique<RomNettouYakyuu>(config, std::move(rom));
		break;
	case HALNOTE:
		result = make_unique<RomHalnote>(config, std::move(rom));
		break;
	case ZEMINA25IN1:
		result = make_unique<RomZemina25in1>(config, std::move(rom));
		break;
	case ZEMINA80IN1:
		result = make_unique<RomZemina80in1>(config, std::move(rom));
		break;
	case ZEMINA90IN1:
		result = make_unique<RomZemina90in1>(config, std::move(rom));
		break;
	case ZEMINA126IN1:
		result = make_unique<RomZemina126in1>(config, std::move(rom));
		break;
	case HOLY_QURAN:
		result = make_unique<RomHolyQuran>(config, std::move(rom));
		break;
	case HOLY_QURAN2:
		result = make_unique<RomHolyQuran2>(config, std::move(rom));
		break;
	case FSA1FM1:
		result = make_unique<RomFSA1FM1>(config, std::move(rom));
		break;
	case FSA1FM2:
		result = make_unique<RomFSA1FM2>(config, std::move(rom));
		break;
	case MANBOW2:
	case MANBOW2_2:
	case HAMARAJANIGHT:
	case MEGAFLASHROMSCC:
	case RBSC_FLASH_KONAMI_SCC:
		result = make_unique<RomManbow2>(config, std::move(rom), type);
		break;
	case MATRAINK:
		result = make_unique<RomMatraInk>(config, std::move(rom));
		break;
	case MATRACOMPILATION:
		result = make_unique<RomMatraCompilation>(config, std::move(rom));
		break;
	case ARC:
		result = make_unique<RomArc>(config, std::move(rom));
		break;
	case ALALAMIAH30IN1:
		result = make_unique<RomAlAlamiah30in1>(config, std::move(rom));
		break;
	case RETROHARD31IN1:
		result = make_unique<RomRetroHard31in1>(config, std::move(rom));
		break;
	case ROMHUNTERMK2:
		result = make_unique<ROMHunterMk2>(config, std::move(rom));
		break;
	case MEGAFLASHROMSCCPLUS:
		result = make_unique<MegaFlashRomSCCPlus>(config, std::move(rom));
		break;
	case REPRO_CARTRIDGE1:
		result = make_unique<ReproCartridgeV1>(config, std::move(rom));
		break;
	case REPRO_CARTRIDGE2:
		result = make_unique<ReproCartridgeV2>(config, std::move(rom));
		break;
	case YAMANOOTO:
		result = make_unique<Yamanooto>(config, std::move(rom));
		break;
	case KONAMI_ULTIMATE_COLLECTION:
		result = make_unique<KonamiUltimateCollection>(config, std::move(rom));
		break;
	case DOOLY:
		result = make_unique<RomDooly>(config, std::move(rom));
		break;
	case MSXTRA:
		result = make_unique<RomMSXtra>(config, std::move(rom));
		break;
	case MULTIROM:
		result = make_unique<RomMultiRom>(config, std::move(rom));
		break;
	case RAMFILE:
		result = make_unique<RomRamFile>(config, std::move(rom));
		break;
	case COLECOMEGACART:
		result = make_unique<RomColecoMegaCart>(config, std::move(rom));
		break;
	default:
		throw MSXException("Unknown ROM type");
	}

	return result;
}

} // namespace openmsx::RomFactory
