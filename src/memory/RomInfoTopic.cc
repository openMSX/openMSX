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

void RomInfoTopic::execute(array_ref<TclObject> tokens, TclObject& result) const
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
		completeString(tokens, RomInfo::getAllRomTypes(), false);
	}
}

} // namespace openmsx
