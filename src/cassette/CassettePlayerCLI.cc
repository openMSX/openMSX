#include "CassettePlayerCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::string;

namespace openmsx {

CassettePlayerCLI::CassettePlayerCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-cassetteplayer", *this);
	parser.registerFileType("cas,wav", *this);
}

void CassettePlayerCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref CassettePlayerCLI::optionHelp() const
{
	return "Put cassette image specified in argument in "
	       "virtual cassetteplayer";
}

void CassettePlayerCLI::parseFileType(const string& filename,
                                      array_ref<string>& /*cmdLine*/)
{
	if (!parser.getGlobalCommandController().hasCommand("cassetteplayer")) {
		throw MSXException("No cassetteplayer.");
	}
	TclObject command;
	command.addListElement("cassetteplayer");
	command.addListElement(filename);
	command.executeCommand(parser.getInterpreter());
}

string_ref CassettePlayerCLI::fileTypeHelp() const
{
	return "Cassette image, raw recording or fMSX CAS image";
}

} // namespace openmsx
