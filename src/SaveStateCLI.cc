#include "SaveStateCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::deque;
using std::string;

namespace openmsx {

SaveStateCLI::SaveStateCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-savestate", *this);
	parser.registerFileClass("openMSX savestate", *this);
}

void SaveStateCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref SaveStateCLI::optionHelp() const
{
	return "Load savestate and start emulation from there";
}

void SaveStateCLI::parseFileType(const string& filename,
                                      deque<string>& /*cmdLine*/)
{
	// TODO: this is basically a C++ version of a part of savestate.tcl.
	// Can that be improved?
	GlobalCommandController& controller = parser.getGlobalCommandController();
	TclObject command(controller.getInterpreter());
	command.addListElement("restore_machine");
	command.addListElement(filename);
	string newId = command.executeCommand();
	command = TclObject(controller.getInterpreter());
	command.addListElement("machine");
	string currentId = command.executeCommand();
	if (currentId != "") {
		command = TclObject(controller.getInterpreter());
		command.addListElement("delete_machine");
		command.addListElement(currentId);
		command.executeCommand();
	}
	command = TclObject(controller.getInterpreter());
	command.addListElement("activate_machine");
	command.addListElement(newId);
	command.executeCommand();
}

string_ref SaveStateCLI::fileTypeHelp() const
{
	return "openMSX savestate";
}

} // namespace openmsx
