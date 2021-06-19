#include "RomInfoTopic.hh"
#include "RomInfo.hh"
#include "TclObject.hh"
#include "CommandException.hh"

using std::vector;
using std::string;

namespace openmsx {

RomInfoTopic::RomInfoTopic(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "romtype")
{
}

void RomInfoTopic::execute(span<const TclObject> tokens, TclObject& result) const
{
	switch (tokens.size()) {
	case 2: {
		result.addListElements(RomInfo::getAllRomTypes());
		break;
	}
	case 3: {
		RomType type = RomInfo::nameToRomType(tokens[2].getString());
		if (type == ROM_UNKNOWN) {
			throw CommandException("Unknown rom type");
		}
		result.addDictKeyValues("description", RomInfo::getDescription(type),
		                        "blocksize", int(RomInfo::getBlockSize(type)));
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string RomInfoTopic::help(span<const TclObject> /*tokens*/) const
{
	return "Shows a list of supported rom types. "
	       "Or show info on a specific rom type.";
}

void RomInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, RomInfo::getAllRomTypes(), false);
	}
}

} // namespace openmsx
