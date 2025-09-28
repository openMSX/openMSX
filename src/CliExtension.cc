#include "CliExtension.hh"

#include "CommandLineParser.hh"
#include "HardwareConfig.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"

#include <cassert>

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	for (const auto* ext : {"-ext", "-exta", "-extb", "-extc", "-extd"}) {
		cmdLineParser.registerOption(ext, *this);
	}
}

void CliExtension::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	try {
		std::string extensionName = getArgument(option, cmdLine);
		MSXMotherBoard* motherboard = cmdLineParser.getMotherBoard();
		assert(motherboard);
		std::string slotName;
		if (option.size() == 5) {
			slotName = option[4];
		} else {
			slotName = "any";
		}
		motherboard->insertExtension(extensionName,
			motherboard->loadExtension(extensionName, slotName));
	} catch (MSXException& e) {
		throw FatalError(std::move(e).getMessage());
	}
}

std::string_view CliExtension::optionHelp() const
{
	return "Insert the extension specified in argument";
}

} // namespace openmsx
