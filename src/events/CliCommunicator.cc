// $Id$

#include <iostream>
#include "CliCommunicator.hh"
#include "CommandController.hh"
#include "Scheduler.hh"

using std::cin;
using std::cout;
using std::flush;

namespace openmsx {

CliCommunicator::CliCommunicator()
{
}

CliCommunicator::~CliCommunicator()
{
}

void CliCommunicator::run()
{
	Scheduler* scheduler = Scheduler::instance();
	while (true) {
		string cmd;
		cin >> cmd;
		cmds.push_back(cmd);
		scheduler->setSyncPoint(Scheduler::ASAP, this);
	}
}

void CliCommunicator::executeUntilEmuTime(const EmuTime& time, int userData)
{
	CommandController* controller = CommandController::instance();
	while (!cmds.empty()) {
		string cmd = cmds.front();
		cmds.pop_front();
		string result;
		try {
			result = controller->executeCommand(cmd);
		} catch (CommandException &e) {
			result = e.getMessage() + '\n';
		}
		cout << result << flush;
	}
}

const string& CliCommunicator::schedName() const
{
	static const string NAME("CliCommunicator");
	return NAME;
}

} // namespace openmsx
