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
	parser.registerFileType({"cas", "wav"}, *this);
}

void CassettePlayerCLI::parseOption(const string& option, span<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view CassettePlayerCLI::optionHelp() const
{
	return "Put cassette image specified in argument in "
	       "virtual cassetteplayer";
}

void CassettePlayerCLI::parseFileType(const string& filename,
                                      span<string>& /*cmdLine*/)
{
	if (!parser.getGlobalCommandController().hasCommand("cassetteplayer")) {
		throw MSXException("No cassette player present.");
	}
	TclObject command = makeTclList("cassetteplayer", filename);
	command.executeCommand(parser.getInterpreter());
}

std::string_view CassettePlayerCLI::fileTypeHelp() const
{
	return "Cassette image, raw recording or fMSX CAS image";
}

std::string_view CassettePlayerCLI::fileTypeCategoryName() const
{
	return "cassette";
}

} // namespace openmsx
