// $Id$

#ifndef STATECHANGEDISTRIBUTOR_HH
#define STATECHANGEDISTRIBUTOR_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include "shared_ptr.hh"
#include <vector>

namespace openmsx {

class StateChangeListener;
class StateChange;

class StateChangeDistributor : private noncopyable
{
public:
	typedef shared_ptr<const StateChange> EventPtr;

	StateChangeDistributor();
	~StateChangeDistributor();

	/** Registers a given object to receive state change events.
	 * @param listener Listener that will be notified when an event arrives.
	 */
	void registerListener(StateChangeListener& listener);

	/** Unregisters a previously registered listener.
	 * @param listener Listener to unregister.
	 */
	void unregisterListener(StateChangeListener& listener);

	/** Deliver the event to all registered listeners
	 * MSX input devices should call the distributeNew() version, only the
	 * replayer should call the distributeReplay() version.
	 * These two different versions are used to detect the transition from
	 * replayed events to live events. Note that a transition from live to
	 * replay is not allowed. This transition should be done by creating a
	 * new StateChangeDistributor object (object always starts in replay
	 * state), but this is automatically taken care of because replay
	 * always starts from a freshly restored snapshot.
	 * @param event The event
	 */
	void distributeNew   (EventPtr event);
	void distributeReplay(EventPtr event);

	/** Explicitly stop replay.
	 * Should be called when replay->live transition cannot be signaled via
	 * a new event, so (only) when we reach the end of the replay log.
	 */
	void stopReplay(EmuTime::param time);

private:
	bool isRegistered(StateChangeListener* listener) const;
	void distribute(EventPtr event);

	typedef std::vector<StateChangeListener*> Listeners;
	Listeners listeners;
	bool replaying;
};

} // namespace openmsx

#endif
