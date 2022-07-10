#include "CassettePlayerCLI.hh"
#include "CommandLineParser.hh"
#include "Interpreter.hh"
#include "MSXException.hh"
#include "TclObject.hh"

namespace openmsx {

CassettePlayerCLI::CassettePlayerCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-cassetteplayer", *this);
	parser.registerFileType({"cas", "wav"}, *this);
}

void CassettePlayerCLI::parseOption(const std::string& option,
                                    std::span<std::string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view CassettePlayerCLI::optionHelp() const
{
	return "Put cassette image specified in argument in "
	       "virtual cassetteplayer";
}

void CassettePlayerCLI::parseFileType(const std::string& filename,
                                      std::span<std::string>& /*cmdLine*/)
{
	if (!parser.getInterpreter().hasCommand("cassetteplayer")) {
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
