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

typedef map<MapperType, string> Description;
static Description description;

RomInfoTopic::RomInfoTopic()
{
	description[GENERIC_8KB] = "Generic 8kB";
	description[GENERIC_16KB] = "Generic 16kB";
	description[KONAMI5] = "Konami with SCC";
	description[KONAMI4] = "Konami MegaROM";
	description[ASCII_8KB] = "ASCII 8kB";
	description[ASCII_16KB] = "ASCII 16kB";
	description[R_TYPE] = "R-Type";
	description[CROSS_BLAIM] = "Cross Blaim";
	description[MSX_AUDIO] = "Panasonic MSX-AUDIO";
	description[HARRY_FOX] = "Harry Fox";
	description[HALNOTE] = "Halnote";
	description[KOREAN80IN1] = "Zemmix 80 in 1";
	description[KOREAN90IN1] = "Zemmix 90 in 1";
	description[KOREAN126IN1] = "Zemmix 126 in 1";
	description[HOLY_QURAN] = "Holy Qu'ran";
	description[PLAIN] = "Plain (any size)";
	description[FSA1FM1] = "Panasonic FS-A1FM internal mapper 1";
	description[FSA1FM2] = "Panasonic FS-A1FM internal mapper 2";
	description[DRAM] = "MSXturboR DRAM";

	// plain variants
	description[PAGE0] = "Plain 16kB page 0";
	description[PAGE1] = "Plain 16kB page 1";
	description[PAGE01] = "Plain 32kB page 0-1";
	description[PAGE2] = "Plain 16kB page 2 (BASIC)";
	description[PAGE12] = "Plain 32kB page 1-2";
	description[PAGE012] = "Plain 48kB page 0-2";
	description[PAGE3] = "Plain 16kB page 3";
	description[PAGE23] = "Plain 32kB page 2-3";
	description[PAGE123] = "Plain 48kB page 1-3";
	description[PAGE0123] = "Plain 64kB";
	
	// with SRAM
	description[ASCII8_8] = "ASCII 8kB with 8kB SRAM";
	description[HYDLIDE2] = "ASCII 16kB with 2kB SRAM";
	description[GAME_MASTER2] = "Konami's Game Master 2";
	description[PANASONIC] = "Panasonic internal mapper";
	description[NATIONAL] = "National internal mapper";
	description[KOEI_8] = "Koei with 8kB SRAM";
	description[KOEI_32] = "Koei with 32kB SRAM";
	description[WIZARDRY] = "Wizardry";
	
	// with DAC
	description[MAJUTSUSHI] = "Hai no Majutsushi";
	description[SYNTHESIZER] = "Konami's Synthesizer";
}

void RomInfoTopic::execute(const vector<CommandArgument>& tokens,
                           CommandArgument& result) const
{
	switch (tokens.size()) {
	case 2: {
		set<string> romTypes;
		RomInfo::getAllMapperTypes(romTypes);
		for (set<string>::const_iterator it = romTypes.begin();
		     it != romTypes.end(); ++it) {
			result.addListElement(*it);
		}
		break;
	}
	case 3: {
		MapperType type = RomInfo::nameToMapperType(tokens[2].getString());
		if (type == UNKNOWN) {
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
	if (tokens.size() == 2) {
		set<string> romTypes;
		RomInfo::getAllMapperTypes(romTypes);
		CommandController::completeString(tokens, romTypes);
	}
}

} // namespace openmsx

