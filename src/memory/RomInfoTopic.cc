// $Id$

#include "RomInfoTopic.hh"
#include "RomInfo.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include <set>

using std::set;
using std::vector;
using std::string;

namespace openmsx {

RomInfoTopic::RomInfoTopic(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "romtype")
{
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
		result.addListElement("description");
		result.addListElement(RomInfo::getDescription(type));
		result.addListElement("blocksize");
		result.addListElement(int(RomInfo::getBlockSize(type)));
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

