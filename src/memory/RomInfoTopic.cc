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
	description[GENERIC_8KB] = "desc";
	/*GENERIC_16KB
	KONAMI5
	KONAMI4
	ASCII_8KB
	ASCII_16KB
	R_TYPE
	CROSS_BLAIM
	MSX_AUDIO
	HARRY_FOX
	HALNOTE
	KOREAN80IN1
	KOREAN90IN1
	KOREAN126IN1
	HOLY_QURAN
	PLAIN
	FSA1FM1
	FSA1FM2
	DRAM

	PAGE0
	PAGE1
	PAGE01
	PAGE2
	PAGE02
	PAGE12
	PAGE012
	PAGE3
	PAGE03
	PAGE13
	PAGE013
	PAGE23
	PAGE023
	PAGE123
	PAGE0123
	
	HAS_SRAM
	ASCII8_8
	HYDLIDE2
	GAME_MASTER2
	PANASONIC
	NATIONAL
	KOEI_8
	KOEI_32
	WIZARDRY

	HAS_DAC
	MAJUTSUSHI
	SYNTHESIZER*/
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

