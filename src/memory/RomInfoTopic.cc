// $Id$

#include "RomInfoTopic.hh"
#include "RomInfo.hh"
#include "CommandArgument.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include <map>
#include <set>

using std::map;
using std::set;
using std::vector;
using std::string;

namespace openmsx {

typedef map<RomType, string> Description;
static Description description;

RomInfoTopic::RomInfoTopic()
{
	description[ROM_GENERIC_8KB] = "Generic 8kB";
	description[ROM_GENERIC_16KB] = "Generic 16kB";
	description[ROM_KONAMI_SCC] = "Konami with SCC";
	description[ROM_KONAMI] = "Konami MegaROM";
	description[ROM_ASCII8] = "ASCII 8kB";
	description[ROM_ASCII16] = "ASCII 16kB";
	description[ROM_R_TYPE] = "R-Type";
	description[ROM_CROSS_BLAIM] = "Cross Blaim";
	description[ROM_MSX_AUDIO] = "Panasonic MSX-AUDIO";
	description[ROM_HARRY_FOX] = "Harry Fox";
	description[ROM_HALNOTE] = "Halnote";
	description[ROM_ZEMINA80IN1] = "Zemina 80 in 1";
	description[ROM_ZEMINA90IN1] = "Zemina 90 in 1";
	description[ROM_ZEMINA126IN1] = "Zemina 126 in 1";
	description[ROM_HOLY_QURAN] = "Holy Qu'ran";
	description[ROM_FSA1FM1] = "Panasonic FS-A1FM internal mapper 1";
	description[ROM_FSA1FM2] = "Panasonic FS-A1FM internal mapper 2";
	description[ROM_DRAM] = "MSXturboR DRAM";

	// plain variants
	description[ROM_MIRRORED] = "Plain rom, mirrored (any size)";
	description[ROM_MIRRORED0000] = "Plain rom, mirrored start at 0x0000";
	description[ROM_MIRRORED4000] = "Plain rom, mirrored start at 0x4000";
	description[ROM_MIRRORED8000] = "Plain rom, mirrored start at 0x8000";
	description[ROM_MIRROREDC000] = "Plain rom, mirrored start at 0xC000";
	description[ROM_NORMAL] = "Plain rom (any size)";
	description[ROM_NORMAL0000] = "Plain rom start at 0x0000";
	description[ROM_NORMAL4000] = "Plain rom start at 0x4000";
	description[ROM_NORMAL8000] = "Plain rom start at 0x8000";
	description[ROM_NORMALC000] = "Plain rom start at 0xC000";
	description[ROM_PAGE0] = "Plain 16kB page 0";
	description[ROM_PAGE1] = "Plain 16kB page 1";
	description[ROM_PAGE01] = "Plain 32kB page 0-1";
	description[ROM_PAGE2] = "Plain 16kB page 2 (BASIC)";
	description[ROM_PAGE12] = "Plain 32kB page 1-2";
	description[ROM_PAGE012] = "Plain 48kB page 0-2";
	description[ROM_PAGE3] = "Plain 16kB page 3";
	description[ROM_PAGE23] = "Plain 32kB page 2-3";
	description[ROM_PAGE123] = "Plain 48kB page 1-3";
	description[ROM_PAGE0123] = "Plain 64kB";
	
	// with SRAM
	description[ROM_ASCII8_8] = "ASCII 8kB with 8kB SRAM";
	description[ROM_ASCII16_2] = "ASCII 16kB with 2kB SRAM";
	description[ROM_GAME_MASTER2] = "Konami's Game Master 2";
	description[ROM_PANASONIC] = "Panasonic internal mapper";
	description[ROM_NATIONAL] = "National internal mapper";
	description[ROM_KOEI_8] = "Koei with 8kB SRAM";
	description[ROM_KOEI_32] = "Koei with 32kB SRAM";
	description[ROM_WIZARDRY] = "Wizardry";
	
	// with DAC
	description[ROM_MAJUTSUSHI] = "Hai no Majutsushi";
	description[ROM_SYNTHESIZER] = "Konami's Synthesizer";
}

void RomInfoTopic::execute(const vector<CommandArgument>& tokens,
                           CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> romTypes;
		RomInfo::getAllRomTypes(romTypes);
		for (set<string>::const_iterator it = romTypes.begin();
		     it != romTypes.end(); ++it) {
			result.addListElement(*it);
		}
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
			result.setString(it->second);
		}
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string RomInfoTopic::help(const vector<string>& tokens) const
{
	return "Shows a list of supported rom types. "
	       "Or show info on a specific rom type.";
}

void RomInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> romTypes;
		RomInfo::getAllRomTypes(romTypes);
		CommandController::completeString(tokens, romTypes, false);
	}
}

} // namespace openmsx

