// $Id$

#include "MSXRomCLI.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"

using std::list;
using std::string;

namespace openmsx {

MSXRomCLI::MSXRomCLI(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	cmdLineParser.registerOption("-ips", &ipsOption);
	cmdLineParser.registerOption("-romtype", &romTypeOption);
	cmdLineParser.registerOption("-cart", this);
	cmdLineParser.registerOption("-carta", this);
	cmdLineParser.registerOption("-cartb", this);
	cmdLineParser.registerOption("-cartc", this);
	cmdLineParser.registerOption("-cartd", this);
	cmdLineParser.registerFileClass("romimage", this);
}

bool MSXRomCLI::parseOption(const string& option, list<string>& cmdLine)
{
	string arg = getArgument(option, cmdLine);
	string slotname;
	if (option.length() == 6) {
		slotname = option[5];
	} else {
		slotname = "any";
	}
	parse(arg, slotname, cmdLine);
	return true;
}

const string& MSXRomCLI::optionHelp() const
{
	static const string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}

void MSXRomCLI::parseFileType(const string& arg, list<string>& cmdLine)
{
	parse(arg, "any", cmdLine);
}

const string& MSXRomCLI::fileTypeHelp() const
{
	static const string text("ROM image of a cartridge");
	return text;
}

void MSXRomCLI::parse(const string& arg, const string& slotname,
                      list<string>& cmdLine)
{
	// parse extra options  -ips  and  -romtype
	std::vector<string> options;
	while (true) {
		string option = peekArgument(cmdLine);
		if ((option == "-ips") || (option == "-romtype")) {
			options.push_back(option);
			cmdLine.pop_front();
			options.push_back(getArgument(option, cmdLine));
		} else {
			break;
		}
	}
	cmdLineParser.getMotherBoard().loadRom(arg, slotname, options);
}

bool MSXRomCLI::IpsOption::parseOption(const string& /*option*/,
                                       list<string>& /*cmdLine*/)
{
	throw FatalError(
		"-ips options should immediately follow a ROM or disk image.");
}

const string& MSXRomCLI::IpsOption::optionHelp() const
{
	static const string text(
		"Apply the given IPS patch to the ROM or disk image in front");
	return text;
}

bool MSXRomCLI::RomTypeOption::parseOption(const string& /*option*/,
                                           list<string>& /*cmdLine*/)
{
	throw FatalError("-romtype options should immediately follow a ROM.");
}

const string& MSXRomCLI::RomTypeOption::optionHelp() const
{
	static const string text("Specify the rom type for the ROM in front");
	return text;
}

} // namespace openmsx
