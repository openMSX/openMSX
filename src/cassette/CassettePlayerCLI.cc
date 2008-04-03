// $Id$

#include "CassettePlayerCLI.hh"
#include "CommandLineParser.hh"
#include "CommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::deque;
using std::string;

namespace openmsx {

CassettePlayerCLI::CassettePlayerCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getCommandController())
{
	commandLineParser.registerOption("-cassetteplayer", *this);
	commandLineParser.registerFileClass("cassetteimage", *this);
}

bool CassettePlayerCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}

const string& CassettePlayerCLI::optionHelp() const
{
	static const string text(
	  "Put cassette image specified in argument in virtual cassetteplayer");
	return text;
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

const string& CassettePlayerCLI::fileTypeHelp() const
{
	static const string text(
		"Cassette image, raw recording or fMSX CAS image");
	return text;
}

} // namespace openmsx
