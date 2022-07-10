#include "SaveStateCLI.hh"
#include "CommandLineParser.hh"
#include "TclObject.hh"

namespace openmsx {

SaveStateCLI::SaveStateCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-savestate", *this);
	parser.registerFileType({"oms"}, *this);
}

void SaveStateCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view SaveStateCLI::optionHelp() const
{
	return "Load savestate and start emulation from there";
}

void SaveStateCLI::parseFileType(const std::string& filename,
                                 std::span<std::string>& /*cmdLine*/)
{
	// TODO: this is basically a C++ version of a part of savestate.tcl.
	// Can that be improved?
	auto& interp = parser.getInterpreter();

	TclObject command1 = makeTclList("restore_machine", filename);
	auto newId = command1.executeCommand(interp);

	TclObject command2 = makeTclList("machine");
	auto currentId = command2.executeCommand(interp);

	if (!currentId.empty()) {
		TclObject command3 = makeTclList("delete_machine", currentId);
		command3.executeCommand(interp);
	}

	TclObject command4 = makeTclList("activate_machine", newId);
	command4.executeCommand(interp);
}

std::string_view SaveStateCLI::fileTypeHelp() const
{
	return "openMSX savestate";
}

std::string_view SaveStateCLI::fileTypeCategoryName() const
{
	return "savestate";
}

} // namespace openmsx
