// $Id$

#include "RecordedCommand.hh"
#include "CommandController.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"
#include "Scheduler.hh"
#include "StateChange.hh"
#include "ScopedAssign.hh"
#include "checked_cast.hh"
#include "unreachable.hh"

using std::vector;
using std::string;

namespace openmsx {


RecordedCommand::RecordedCommand(CommandController& commandController,
                                 StateChangeDistributor& stateChangeDistributor_,
                                 Scheduler& scheduler_,
                                 const string& name)
	: Command(commandController, name)
	, stateChangeDistributor(stateChangeDistributor_)
	, scheduler(scheduler_)
	, dummyResultObject(new TclObject(getInterpreter()))
	, currentResultObject(dummyResultObject.get())
{
	stateChangeDistributor.registerListener(*this);
}

RecordedCommand::~RecordedCommand()
{
	stateChangeDistributor.unregisterListener(*this);
}

void RecordedCommand::execute(const vector<TclObject*>& tokens,
                              TclObject& result)
{
	EmuTime::param time = scheduler.getCurrentTime();
	if (needRecord(tokens)) {
		ScopedAssign<TclObject*> sa(currentResultObject, &result);
		stateChangeDistributor.distributeNew(
			StateChangeDistributor::EventPtr(
				new MSXCommandEvent(tokens, time)));
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

void RecordedCommand::signalStateChange(shared_ptr<const StateChange> event)
{
	const MSXCommandEvent* commandEvent =
		dynamic_cast<const MSXCommandEvent*>(event.get());
	if (!commandEvent) return;

	const vector<TclObject*>& tokens = commandEvent->getTokens();
	if (getBaseName(tokens[0]->getString()) != getName()) return;

	execute(tokens, *currentResultObject, commandEvent->getTime());
}

void RecordedCommand::stopReplay(EmuTime::param /*time*/)
{
	// nothing
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


// class MSXCommandEvent

MSXCommandEvent::MSXCommandEvent(const vector<string>& tokens_, EmuTime::param time)
	: StateChange(time)
{
	for (vector<string>::const_iterator it = tokens_.begin();
	     it != tokens_.end(); ++it) {
		tokens.push_back(new TclObject(0, *it));
	}
}

MSXCommandEvent::MSXCommandEvent(const vector<TclObject*>& tokens_, EmuTime::param time)
	: StateChange(time)
{
	for (vector<TclObject*>::const_iterator it = tokens_.begin();
	     it != tokens_.end(); ++it) {
		tokens.push_back(new TclObject(**it));
	}
}

MSXCommandEvent::~MSXCommandEvent()
{
	for (vector<TclObject*>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		delete *it;
	}
}

const vector<TclObject*>& MSXCommandEvent::getTokens() const
{
	return tokens;
}

} // namespace openmsx
