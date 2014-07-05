#include "SaveStateCLI.hh"
#include "CommandLineParser.hh"
#include "TclObject.hh"

using std::string;

namespace openmsx {

SaveStateCLI::SaveStateCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-savestate", *this);
	parser.registerFileType("oms", *this);
}

void SaveStateCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref SaveStateCLI::optionHelp() const
{
	return "Load savestate and start emulation from there";
}

void SaveStateCLI::parseFileType(const string& filename,
                                 array_ref<string>& /*cmdLine*/)
{
	// TODO: this is basically a C++ version of a part of savestate.tcl.
	// Can that be improved?
	auto& interp = parser.getInterpreter();

	TclObject command1({string_ref("restore_machine"),
	                    string_ref(filename)});
	string newId = command1.executeCommand(interp);

	TclObject command2({"machine"});
	string currentId = command2.executeCommand(interp);

	if (!currentId.empty()) {
		TclObject command3({string_ref("delete_machine"),
		                    string_ref(currentId)});
		command3.executeCommand(interp);
	}

	TclObject command4({string_ref("activate_machine"),
	                    string_ref(newId)});
	command4.executeCommand(interp);
}

string_ref SaveStateCLI::fileTypeHelp() const
{
	return "openMSX savestate";
}

} // namespace openmsx
