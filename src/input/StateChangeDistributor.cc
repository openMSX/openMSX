// $Id$

#include "StateChangeDistributor.hh"
#include "StateChangeListener.hh"
#include "StateChange.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

StateChangeDistributor::StateChangeDistributor()
	: recorder(NULL)
	, replaying(true)
{
}

StateChangeDistributor::~StateChangeDistributor()
{
	assert(listeners.empty());
}

bool StateChangeDistributor::isRegistered(StateChangeListener* listener) const
{
	return find(listeners.begin(), listeners.end(), listener) !=
	       listeners.end();
}

void StateChangeDistributor::registerListener(StateChangeListener& listener)
{
	assert(!isRegistered(&listener));
	listeners.push_back(&listener);
}

void StateChangeDistributor::unregisterListener(StateChangeListener& listener)
{
	assert(isRegistered(&listener));
	listeners.erase(find(listeners.begin(), listeners.end(), &listener));
}

void StateChangeDistributor::registerRecorder(StateChangeListener& recorder_)
{
	assert(recorder == NULL);
	recorder = &recorder_;
}

void StateChangeDistributor::unregisterRecorder(StateChangeListener& recorder_)
{
	(void)recorder_;
	assert(recorder == &recorder_);
	recorder = NULL;
}

void StateChangeDistributor::distributeNew(EventPtr event)
{
	if (replaying) {
		stopReplay(event->getTime());
	}
	distribute(event);
}

void StateChangeDistributor::distributeReplay(EventPtr event)
{
	assert(replaying);
	distribute(event);
}

void StateChangeDistributor::distribute(EventPtr event)
{
	// Iterate over a copy because signalStateChange() may indirect call
	// back into registerListener().
	//   e.g. signalStateChange() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerListener()
	if (recorder) recorder->signalStateChange(event);
	Listeners copy = listeners;
	for (Listeners::const_iterator it = copy.begin();
	     it != copy.end(); ++it) {
		if (isRegistered(*it)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			(*it)->signalStateChange(event);
		}
	}
}

void StateChangeDistributor::stopReplay(EmuTime::param time)
{
	replaying = false;
	if (recorder) recorder->stopReplay(time);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->stopReplay(time);
	}
}

} // namespace openmsx
