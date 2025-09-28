#include "RomInfoTopic.hh"

#include "CommandException.hh"
#include "RomInfo.hh"
#include "TclObject.hh"

namespace openmsx {

RomInfoTopic::RomInfoTopic(InfoCommand& openMSXInfoCommand)
	: InfoTopic(openMSXInfoCommand, "romtype")
{
}

void RomInfoTopic::execute(std::span<const TclObject> tokens, TclObject& result) const
{
	switch (tokens.size()) {
	case 2: {
		result.addListElements(RomInfo::getAllRomTypes());
		break;
	}
	case 3: {
		RomType type = RomInfo::nameToRomType(tokens[2].getString());
		if (type == RomType::UNKNOWN) {
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

std::string RomInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Shows a list of supported rom types. "
	       "Or show info on a specific rom type.";
}

void RomInfoTopic::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, RomInfo::getAllRomTypes(), false);
	}
}

} // namespace openmsx
