// $Id$

#include "RomInfoTopic.hh"
#include "RomInfo.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include <map>
#include <set>

using std::set;
using std::vector;
using std::string;

namespace openmsx {

typedef std::map<RomType, string> Description;
typedef std::map<RomType, int> BlockSize;
static Description description;
static BlockSize blocksize;

RomInfoTopic::RomInfoTopic(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "romtype")
{
	description[ROM_GENERIC_8KB] = "Generic 8kB";
	description[ROM_GENERIC_16KB] = "Generic 16kB";
	description[ROM_KONAMI_SCC] = "Konami with SCC";
	description[ROM_KONAMI] = "Konami MegaROM";
	description[ROM_KBDMASTER] = "Konami Keyboard Master with VLM5030";
	description[ROM_ASCII8] = "ASCII 8kB";
	description[ROM_ASCII16] = "ASCII 16kB";
	description[ROM_PADIAL8] = "Padial 8kB";
	description[ROM_PADIAL16] = "Padial 16kB";
	description[ROM_SUPERLODERUNNER] = "Super Lode Runner";
	description[ROM_R_TYPE] = "R-Type";
	description[ROM_CROSS_BLAIM] = "Cross Blaim";
	description[ROM_MSXDOS2] = "MSX-DOS2";
	description[ROM_MSX_AUDIO] = "MSX-Audio";
	description[ROM_HARRY_FOX] = "Harry Fox";
	description[ROM_HALNOTE] = "Halnote";
	description[ROM_ZEMINA80IN1] = "Zemina 80 in 1";
	description[ROM_ZEMINA90IN1] = "Zemina 90 in 1";
	description[ROM_ZEMINA126IN1] = "Zemina 126 in 1";
	description[ROM_HOLY_QURAN]  = "Holy Qu'ran (pre-decrypted)";
	description[ROM_HOLY_QURAN2] = "Holy Qu'ran";
	description[ROM_FSA1FM1] = "Panasonic FS-A1FM internal mapper 1";
	description[ROM_FSA1FM2] = "Panasonic FS-A1FM internal mapper 2";
	description[ROM_DRAM] = "MSXturboR DRAM";
	description[ROM_MANBOW2] = "Manbow2";
	description[ROM_MANBOW2_2] = "Manbow2 - Second Release";
	description[ROM_HAMARAJANIGHT] = "Best of Hamaraja Night";
	description[ROM_MEGAFLASHROMSCC] = "Mega Flash ROM SCC";
	description[ROM_MATRAINK] = "Matra Ink";
	description[ROM_ARC] = "Parallax' ARC";
	description[ROM_DOOLY] = "Baby Dinosaur Dooly";
	description[ROM_MSXTRA] = "PTC MSXtra";
	description[ROM_MULTIROM] = "MultiRom Collection";

	description[ROM_MIRRORED] = "Plain rom, mirrored (any size)";
	description[ROM_MIRRORED0000] = "Plain rom, mirrored start at 0x0000";
	description[ROM_MIRRORED2000] = "Plain rom, mirrored start at 0x2000";
	description[ROM_MIRRORED4000] = "Plain rom, mirrored start at 0x4000";
	description[ROM_MIRRORED6000] = "Plain rom, mirrored start at 0x6000";
	description[ROM_MIRRORED8000] = "Plain rom, mirrored start at 0x8000";
	description[ROM_MIRROREDA000] = "Plain rom, mirrored start at 0xA000";
	description[ROM_MIRROREDC000] = "Plain rom, mirrored start at 0xC000";
	description[ROM_MIRROREDE000] = "Plain rom, mirrored start at 0xE000";
	description[ROM_NORMAL] = "Plain rom (any size)";
	description[ROM_NORMAL0000] = "Plain rom start at 0x0000";
	description[ROM_NORMAL2000] = "Plain rom start at 0x2000";
	description[ROM_NORMAL4000] = "Plain rom start at 0x4000";
	description[ROM_NORMAL6000] = "Plain rom start at 0x6000";
	description[ROM_NORMAL8000] = "Plain rom start at 0x8000";
	description[ROM_NORMALA000] = "Plain rom start at 0xA000";
	description[ROM_NORMALC000] = "Plain rom start at 0xC000";
	description[ROM_NORMALE000] = "Plain rom start at 0xE000";
	description[ROM_PAGE0]    = "Plain 16kB page 0";
	description[ROM_PAGE1]    = "Plain 16kB page 1";
	description[ROM_PAGE2]    = "Plain 16kB page 2 (BASIC)";
	description[ROM_PAGE3]    = "Plain 16kB page 3";
	description[ROM_PAGE01]   = "Plain 32kB page 0-1";
	description[ROM_PAGE02]   = "Plain 32kB page 0,2";
	description[ROM_PAGE03]   = "Plain 32kB page 0,3";
	description[ROM_PAGE12]   = "Plain 32kB page 1-2";
	description[ROM_PAGE13]   = "Plain 32kB page 1,3";
	description[ROM_PAGE23]   = "Plain 32kB page 2-3";
	description[ROM_PAGE012]  = "Plain 48kB page 0-2";
	description[ROM_PAGE013]  = "Plain 48kB page 0-1,3";
	description[ROM_PAGE023]  = "Plain 48kB page 0,2-3";
	description[ROM_PAGE123]  = "Plain 48kB page 1-3";
	description[ROM_PAGE0123] = "Plain 64kB";

	description[ROM_ASCII8_8] = "ASCII 8kB with 8kB SRAM";
	description[ROM_ASCII16_2] = "ASCII 16kB with 2kB SRAM";
	description[ROM_GAME_MASTER2] = "Konami's Game Master 2";
	description[ROM_PANASONIC] = "Panasonic internal mapper";
	description[ROM_NATIONAL] = "National internal mapper";
	description[ROM_KOEI_8] = "Koei with 8kB SRAM";
	description[ROM_KOEI_32] = "Koei with 32kB SRAM";
	description[ROM_WIZARDRY] = "Wizardry";

	description[ROM_MAJUTSUSHI] = "Hai no Majutsushi";
	description[ROM_SYNTHESIZER] = "Konami's Synthesizer";
	description[ROM_PLAYBALL] = "Sony's PlayBall";
	description[ROM_NETTOU_YAKYUU] = "Nettou Yakuu";

	blocksize[ROM_GENERIC_8KB] = 0x2000;
	blocksize[ROM_GENERIC_16KB] = 0x4000;
	blocksize[ROM_KONAMI_SCC] = 0x2000;
	blocksize[ROM_KONAMI] = 0x2000;
	blocksize[ROM_KBDMASTER] = 0x4000;	// officially plain 16K
	blocksize[ROM_ASCII8] = 0x2000;
	blocksize[ROM_ASCII16] = 0x4000;
	blocksize[ROM_PADIAL8] = 0x2000;
	blocksize[ROM_PADIAL16] = 0x4000;
	blocksize[ROM_SUPERLODERUNNER] = 0x4000;
	blocksize[ROM_R_TYPE] = 0x4000;
	blocksize[ROM_CROSS_BLAIM] = 0x4000;
	blocksize[ROM_MSXDOS2] = 0x4000;
	blocksize[ROM_MSX_AUDIO] = 0x0000;	// todo: romblocks debuggable
	blocksize[ROM_HARRY_FOX] = 0x4000;
	blocksize[ROM_HALNOTE] = 0x2000;
	blocksize[ROM_ZEMINA80IN1] = 0x2000;
	blocksize[ROM_ZEMINA90IN1] = 0x2000;
	blocksize[ROM_ZEMINA126IN1] = 0x2000;
	blocksize[ROM_HOLY_QURAN]  = 0x2000;
	blocksize[ROM_HOLY_QURAN2] = 0x2000;
	blocksize[ROM_FSA1FM1] = 0x0000;	// todo: romblocks debuggable?
	blocksize[ROM_FSA1FM2] = 0x2000;
	blocksize[ROM_DRAM] = 0x2000;
	blocksize[ROM_MANBOW2] = 0x2000;
	blocksize[ROM_MANBOW2_2] = 0x2000;
	blocksize[ROM_HAMARAJANIGHT] = 0x2000;
	blocksize[ROM_MEGAFLASHROMSCC] = 0x2000;
	blocksize[ROM_MATRAINK] = 0x0000;
	blocksize[ROM_ARC] = 0x4000;		// officially plain 32K
	blocksize[ROM_DOOLY] = 0x4000;		// officially 32K blocksize, but spread over 2 pages
	blocksize[ROM_MSXTRA] = 0x0000;
	blocksize[ROM_MULTIROM] = 0x0000;

	blocksize[ROM_MIRRORED] = 0x2000;
	blocksize[ROM_MIRRORED0000] = 0x2000;
	blocksize[ROM_MIRRORED2000] = 0x2000;
	blocksize[ROM_MIRRORED4000] = 0x2000;
	blocksize[ROM_MIRRORED6000] = 0x2000;
	blocksize[ROM_MIRRORED8000] = 0x2000;
	blocksize[ROM_MIRROREDA000] = 0x2000;
	blocksize[ROM_MIRROREDC000] = 0x2000;
	blocksize[ROM_MIRROREDE000] = 0x2000;
	blocksize[ROM_NORMAL] = 0x2000;
	blocksize[ROM_NORMAL0000] = 0x2000;
	blocksize[ROM_NORMAL2000] = 0x2000;
	blocksize[ROM_NORMAL4000] = 0x2000;
	blocksize[ROM_NORMAL6000] = 0x2000;
	blocksize[ROM_NORMAL8000] = 0x2000;
	blocksize[ROM_NORMALA000] = 0x2000;
	blocksize[ROM_NORMALC000] = 0x2000;
	blocksize[ROM_NORMALE000] = 0x2000;
	blocksize[ROM_PAGE0]    = 0x2000;
	blocksize[ROM_PAGE1]    = 0x2000;
	blocksize[ROM_PAGE2]    = 0x2000;
	blocksize[ROM_PAGE3]    = 0x2000;
	blocksize[ROM_PAGE01]   = 0x2000;
	blocksize[ROM_PAGE02]   = 0x2000;
	blocksize[ROM_PAGE03]   = 0x2000;
	blocksize[ROM_PAGE12]   = 0x2000;
	blocksize[ROM_PAGE13]   = 0x2000;
	blocksize[ROM_PAGE23]   = 0x2000;
	blocksize[ROM_PAGE012]  = 0x2000;
	blocksize[ROM_PAGE013]  = 0x2000;
	blocksize[ROM_PAGE023]  = 0x2000;
	blocksize[ROM_PAGE123]  = 0x2000;
	blocksize[ROM_PAGE0123] = 0x2000;

	blocksize[ROM_ASCII8_8] = 0x2000;
	blocksize[ROM_ASCII16_2] = 0x4000;
	blocksize[ROM_GAME_MASTER2] = 0x1000;
	blocksize[ROM_PANASONIC] = 0x2000;
	blocksize[ROM_NATIONAL] = 0x4000;
	blocksize[ROM_KOEI_8] = 0x2000;
	blocksize[ROM_KOEI_32] = 0x2000;
	blocksize[ROM_WIZARDRY] = 0x2000;

	blocksize[ROM_MAJUTSUSHI] = 0x2000;
	blocksize[ROM_SYNTHESIZER] = 0x4000;	// officially plain 32K
	blocksize[ROM_PLAYBALL] = 0x4000;	// officially plain 32K
	blocksize[ROM_NETTOU_YAKYUU] = 0x2000;
}

void RomInfoTopic::execute(const vector<TclObject>& tokens,
                           TclObject& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> romTypes;
		RomInfo::getAllRomTypes(romTypes);
		result.addListElements(romTypes.begin(), romTypes.end());
		break;
	}
	case 3: {
		RomType type = RomInfo::nameToRomType(tokens[2].getString());
		if (type == ROM_UNKNOWN) {
			throw CommandException("Unknown rom type");
		}
		Description::const_iterator it = description.find(type);
		if (it == description.end()) {
			result.setString("no info available (TODO)");
		} else {
			BlockSize::const_iterator size = blocksize.find(type);
			result.addListElement("description");
			result.addListElement(it->second);
			result.addListElement("blocksize");
			result.addListElement(size->second);
		}
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string RomInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of supported rom types. "
	       "Or show info on a specific rom type.";
}

void RomInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> romTypes;
		RomInfo::getAllRomTypes(romTypes);
		completeString(tokens, romTypes, false);
	}
}

} // namespace openmsx

