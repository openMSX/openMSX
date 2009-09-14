// $Id$

#include "RecordedCommand.hh"
#include "CommandController.hh"
#include "MSXEventDistributor.hh"
#include "TclObject.hh"
#include "Scheduler.hh"
#include "InputEvents.hh"
#include "ScopedAssign.hh"
#include "checked_cast.hh"
#include "unreachable.hh"

using std::vector;
using std::string;

namespace openmsx {

RecordedCommand::RecordedCommand(CommandController& commandController,
                                 MSXEventDistributor& msxEventDistributor_,
                                 Scheduler& scheduler_,
                                 const string& name)
	: Command(commandController, name)
	, msxEventDistributor(msxEventDistributor_)
	, scheduler(scheduler_)
	, dummyResultObject(new TclObject(commandController.getInterpreter()))
	, currentResultObject(dummyResultObject.get())
{
	msxEventDistributor.registerEventListener(*this);
}

RecordedCommand::~RecordedCommand()
{
	msxEventDistributor.unregisterEventListener(*this);
}

void RecordedCommand::execute(const vector<TclObject*>& tokens,
                              TclObject& result)
{
	EmuTime::param time = scheduler.getCurrentTime();
	if (needRecord(tokens)) {
		ScopedAssign<TclObject*> sa(currentResultObject, &result);
		msxEventDistributor.distributeEvent(
			MSXEventDistributor::EventPtr(
				new MSXCommandEvent(tokens)),
			time);
	} else {
		execute(tokens, result, time);
	}
}

bool RecordedCommand::needRecord(const vector<TclObject*>& tokens) const
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<TclObject*>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back((*it)->getString());
	}
	return needRecord(strings);
}

bool RecordedCommand::needRecord(const vector<string>& /*tokens*/) const
{
	return true;
}

static string getBaseName(const std::string& str)
{
	string::size_type pos = str.rfind("::");
	return (pos == string::npos) ? str : str.substr(pos + 2);
}

void RecordedCommand::signalEvent(
	shared_ptr<const Event> event, EmuTime::param time)
{
	if (event->getType() != OPENMSX_MSX_COMMAND_EVENT) return;
	const MSXCommandEvent* commandEvent =
		checked_cast<const MSXCommandEvent*>(event.get());
	const vector<TclObject*>& tokens = commandEvent->getTokens();
	if (getBaseName(tokens[0]->getString()) != getName()) return;

	execute(tokens, *currentResultObject, time);
}

void RecordedCommand::execute(const vector<TclObject*>& tokens,
                              TclObject& result, EmuTime::param time)
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<TclObject*>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back((*it)->getString());
	}
	result.setString(execute(strings, time));
}

string RecordedCommand::execute(const vector<string>& /*tokens*/,
                                EmuTime::param /*time*/)
{
	// either this method or the method above should be reimplemented
	// by the subclasses
	UNREACHABLE; return "";
}

} // namespace openmsx
