// $Id$

#include <iostream>
#include <string>
#include "CliCommunicator.hh"
#include "CommandController.hh"

using std::cin;
using std::cout;
using std::flush;
using std::string;

namespace openmsx {

void CliCommunicator::run()
{
	CommandController* controller = CommandController::instance();
	while (true) {
		string cmd;
		cin >> cmd;
		string result = controller->executeCommand(cmd);
		cout << result << flush;
	}
}

} // namespace openmsx
