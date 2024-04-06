#include "MSXRomCLI.hh"
#include "CommandLineParser.hh"
#include "HardwareConfig.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include <cassert>

using std::string;

namespace openmsx {

std::span<const std::string_view> MSXRomCLI::getExtensions()
{
	static constexpr std::array<std::string_view, 5> extensions = {
		"ri", "rom", "mx1", "mx2", "sg"
	};
	return extensions;
}

MSXRomCLI::MSXRomCLI(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	cmdLineParser.registerOption("-ips", ipsOption);
	cmdLineParser.registerOption("-romtype", romTypeOption);
	for (const auto* cart : {"-cart", "-carta", "-cartb", "-cartc", "-cartd"}) {
		cmdLineParser.registerOption(cart, *this);
	}
	cmdLineParser.registerFileType(getExtensions(), *this);
}

void MSXRomCLI::parseOption(const string& option, std::span<string>& cmdLine)
{
	string arg = getArgument(option, cmdLine);
	string slotName;
	if (option.size() == 6) {
		slotName = option[5];
	} else {
		slotName = "any";
	}
	parse(arg, slotName, cmdLine);
}

std::string_view MSXRomCLI::optionHelp() const
{
	return "Insert the ROM file (cartridge) specified in argument";
}

void MSXRomCLI::parseFileType(const string& arg, std::span<string>& cmdLine)
{
	parse(arg, "any", cmdLine);
}

std::string_view MSXRomCLI::fileTypeHelp() const
{
	return "ROM image of a cartridge";
}

std::string_view MSXRomCLI::fileTypeCategoryName() const
{
	return "rom";
}

void MSXRomCLI::parse(const string& arg, const string& slotName,
                      std::span<string>& cmdLine)
{
	// parse extra options  -ips  and  -romtype
	std::vector<TclObject> options;
	while (true) {
		string option = peekArgument(cmdLine);
		if (option == one_of("-ips", "-romtype")) {
			options.emplace_back(option);
			cmdLine = cmdLine.subspan(1);
			options.emplace_back(getArgument(option, cmdLine));
		} else {
			break;
		}
	}
	MSXMotherBoard* motherboard = cmdLineParser.getMotherBoard();
	assert(motherboard);
	motherboard->insertExtension("ROM",
		HardwareConfig::createRomConfig(*motherboard, arg, slotName, options));
}

void MSXRomCLI::IpsOption::parseOption(const string& /*option*/,
                                       std::span<string>& /*cmdLine*/)
{
	throw FatalError(
		"-ips options should immediately follow a ROM or disk image.");
}

std::string_view MSXRomCLI::IpsOption::optionHelp() const
{
	return "Apply the given IPS patch to the ROM or disk image specified "
	       "in the preceding option";
}

void MSXRomCLI::RomTypeOption::parseOption(const string& /*option*/,
                                           std::span<string>& /*cmdLine*/)
{
	throw FatalError("-romtype options should immediately follow a ROM.");
}

std::string_view MSXRomCLI::RomTypeOption::optionHelp() const
{
	return "Specify the rom type for the ROM image specified in the "
	       "preceding option";
}

} // namespace openmsx
