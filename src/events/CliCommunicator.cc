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
	: lock(1)
{
}

CliCommunicator::~CliCommunicator()
{
}

void CliCommunicator::run()
{
	Scheduler* scheduler = Scheduler::instance();
	string cmd;
	while (getline(cin, cmd)) {
		lock.down();
		cmds.push_back(cmd);
		lock.up();
		scheduler->setSyncPoint(Scheduler::ASAP, this);
	}
}

void CliCommunicator::executeUntilEmuTime(const EmuTime& time, int userData)
{
	CommandController* controller = CommandController::instance();
	lock.down();
	while (!cmds.empty()) {
		string result;
		try {
			result = controller->executeCommand(cmds.front());
		} catch (CommandException &e) {
			result = e.getMessage() + '\n';
		}
		cout << result << flush;
		cmds.pop_front();
	}
	lock.up();
}

const string& CliCommunicator::schedName() const
{
	static const string NAME("CliCommunicator");
	return NAME;
}

} // namespace openmsx
