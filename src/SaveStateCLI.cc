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

string_view SaveStateCLI::optionHelp() const
{
	return "Load savestate and start emulation from there";
}

void SaveStateCLI::parseFileType(const string& filename,
                                 array_ref<string>& /*cmdLine*/)
{
	// TODO: this is basically a C++ version of a part of savestate.tcl.
	// Can that be improved?
	auto& interp = parser.getInterpreter();

	TclObject command1;
	command1.addListElement("restore_machine");
	command1.addListElement(filename);
	auto newId = command1.executeCommand(interp);

	TclObject command2;
	command2.addListElement("machine");
	auto currentId = command2.executeCommand(interp);

	if (!currentId.empty()) {
		TclObject command3;
		command3.addListElement("delete_machine");
		command3.addListElement(currentId);
		command3.executeCommand(interp);
	}

	TclObject command4;
	command4.addListElement("activate_machine");
	command4.addListElement(newId);
	command4.executeCommand(interp);
}

string_view SaveStateCLI::fileTypeHelp() const
{
	return "openMSX savestate";
}

} // namespace openmsx
