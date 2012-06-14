// $Id$

#include "CassettePlayerCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::deque;
using std::string;

namespace openmsx {

CassettePlayerCLI::CassettePlayerCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getGlobalCommandController())
{
	commandLineParser.registerOption("-cassetteplayer", *this);
	commandLineParser.registerFileClass("cassetteimage", *this);
}

bool CassettePlayerCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}

string_ref CassettePlayerCLI::optionHelp() const
{
	return "Put cassette image specified in argument in "
	       "virtual cassetteplayer";
}

void CassettePlayerCLI::parseFileType(const string& filename,
                                      deque<string>& /*cmdLine*/)
{
	if (!commandController.hasCommand("cassetteplayer")) {
		throw MSXException("No cassetteplayer.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement("cassetteplayer");
	command.addListElement(filename);
	command.executeCommand();
}

string_ref CassettePlayerCLI::fileTypeHelp() const
{
	return "Cassette image, raw recording or fMSX CAS image";
}

} // namespace openmsx
