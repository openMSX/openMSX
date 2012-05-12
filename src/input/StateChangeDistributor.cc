// $Id$

#include "StateChangeDistributor.hh"
#include "StateChangeListener.hh"
#include "StateChange.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

StateChangeDistributor::StateChangeDistributor()
	: recorder(NULL)
	, viewOnlyMode(false)
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

void StateChangeDistributor::registerRecorder(StateChangeRecorder& recorder_)
{
	assert(recorder == NULL);
	recorder = &recorder_;
}

void StateChangeDistributor::unregisterRecorder(StateChangeRecorder& recorder_)
{
	(void)recorder_;
	assert(recorder == &recorder_);
	recorder = NULL;
}

void StateChangeDistributor::distributeNew(const EventPtr& event)
{
	if (viewOnlyMode && isReplaying()) return;

	if (isReplaying()) {
		stopReplay(event->getTime());
	}
	distribute(event);
}

void StateChangeDistributor::distributeReplay(const EventPtr& event)
{
	assert(isReplaying());
	distribute(event);
}

void StateChangeDistributor::distribute(const EventPtr& event)
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
	if (!isReplaying()) return;

	if (recorder) recorder->stopReplay(time);
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->stopReplay(time);
	}
}

void StateChangeDistributor::setViewOnlyMode(bool value)
{
	viewOnlyMode = value;
}

bool StateChangeDistributor::isViewOnlyMode() const
{
	return viewOnlyMode;
}

bool StateChangeDistributor::isReplaying() const
{
	if (recorder) {
		return recorder->isReplaying();
	}
	return false;
}

} // namespace openmsx
