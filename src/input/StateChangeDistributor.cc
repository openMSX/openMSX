#include "StateChangeDistributor.hh"

#include "StateChangeListener.hh"
#include "StateChange.hh"

#include "stl.hh"

#include <cassert>

namespace openmsx {

StateChangeDistributor::~StateChangeDistributor()
{
	assert(listeners.empty());
}

bool StateChangeDistributor::isRegistered(StateChangeListener* listener) const
{
	return contains(listeners, listener);
}

void StateChangeDistributor::registerListener(StateChangeListener& listener)
{
	assert(!isRegistered(&listener));
	listeners.push_back(&listener);
}

void StateChangeDistributor::unregisterListener(StateChangeListener& listener)
{
	move_pop_back(listeners, rfind_unguarded(listeners, &listener));
}

void StateChangeDistributor::registerRecorder(ReverseManager& recorder_)
{
	assert(!recorder);
	recorder = &recorder_;
}

void StateChangeDistributor::unregisterRecorder(ReverseManager& recorder_)
{
	(void)recorder_;
	assert(recorder == &recorder_);
	recorder = nullptr;
}

void StateChangeDistributor::distribute(const StateChange& event) const
{
	// Iterate over a copy because signalStateChange() may indirect call
	// back into registerListener().
	//   e.g. signalStateChange() -> .. -> PlugCmd::execute() -> .. ->
	//        Connector::plug() -> .. -> Joystick::plugHelper() ->
	//        registerListener()
	auto copy = listeners;
	for (auto& l : copy) {
		if (isRegistered(l)) {
			// it's possible the listener unregistered itself
			// (but is still present in the copy)
			l->signalStateChange(event); // might throw
		}
	}
}

void StateChangeDistributor::stopReplay(EmuTime::param time)
{
	if (!isReplaying()) return;

	if (recorder) recorder->stopReplay(time);
	for (auto& l : listeners) {
		l->stopReplay(time);
	}
}

bool StateChangeDistributor::isReplaying() const
{
	if (recorder) {
		return recorder->isReplaying();
	}
	return false;
}

} // namespace openmsx
