// $Id: $

#include "RecordedCommand.hh"
#include "MSXEventDistributor.hh"
#include "Scheduler.hh"
#include "InputEvents.hh"
#include "checked_cast.hh"

using std::vector;
using std::string;

namespace openmsx {

RecordedCommand::RecordedCommand(CommandController& commandController,
                                 MSXEventDistributor& msxEventDistributor_,
                                 Scheduler& scheduler_,
                                 const string& name)
	: SimpleCommand(commandController, name)
	, msxEventDistributor(msxEventDistributor_)
	, scheduler(scheduler_)
{
	msxEventDistributor.registerEventListener(*this);
}

RecordedCommand::~RecordedCommand()
{
	msxEventDistributor.unregisterEventListener(*this);
}

string RecordedCommand::execute(const vector<string>& tokens)
{
	if (needRecord(tokens)) {
		msxEventDistributor.distributeEvent(
			MSXEventDistributor::EventPtr(
				new MSXCommandEvent(tokens)),
			scheduler.getCurrentTime());
		return resultString;
	} else {
		return execute(tokens, scheduler.getCurrentTime());
	}
}

bool RecordedCommand::needRecord(const vector<string>& /*tokens*/) const
{
	return true;
}

void RecordedCommand::signalEvent(
	shared_ptr<const Event> event, const EmuTime& time)
{
	if (event->getType() != OPENMSX_MSX_COMMAND_EVENT) return;
	const MSXCommandEvent* commandEvent =
		checked_cast<const MSXCommandEvent*>(event.get());
	const vector<string>& tokens = commandEvent->getTokens();
	if (tokens[0] != getName()) return;

	resultString = execute(tokens, time);
}

} // namespace openmsx
