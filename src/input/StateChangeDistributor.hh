#ifndef STATECHANGEDISTRIBUTOR_HH
#define STATECHANGEDISTRIBUTOR_HH

#include "EmuTime.hh"
#include <memory>
#include <vector>

namespace openmsx {

class StateChangeListener;
class StateChangeRecorder;
class StateChange;

class StateChangeDistributor
{
public:
	using EventPtr = std::shared_ptr<StateChange>;

	StateChangeDistributor();
	~StateChangeDistributor();

	/** (Un)registers the given object to receive state change events.
	 * @param listener Listener that will be notified when an event arrives.
	 */
	void registerListener  (StateChangeListener& listener);
	void unregisterListener(StateChangeListener& listener);

	/** (Un)registers the given object to receive state change events.
	 * @param recorder Listener that will be notified when an event arrives.
	 * These two methods are very similar to the two above. The difference
	 * is that there can be at most one registered recorder. This recorder
	 * object is always the first object that gets informed about state
	 * changing events.
	 */
	void registerRecorder  (StateChangeRecorder& recorder);
	void unregisterRecorder(StateChangeRecorder& recorder);

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
	void distributeNew   (const EventPtr& event);
	void distributeReplay(const EventPtr& event);

	/** Explicitly stop replay.
	 * Should be called when replay->live transition cannot be signaled via
	 * a new event, so for example when we reach the end of the replay log.
	 * It's OK to call this method when replay was already stopped, in that
	 * case this call has no effect.
	 */
	void stopReplay(EmuTime::param time);

	/**
	 * Set viewOnlyMode. Call this if you don't want distributeNew events
	 * to stop replaying and go to live events (value=true).
	 * @param value false if new events stop replay mode
	 */
	void setViewOnlyMode(bool value) { viewOnlyMode = value; }
	bool isViewOnlyMode() const { return viewOnlyMode; }

	bool isReplaying() const;

private:
	bool isRegistered(StateChangeListener* listener) const;
	void distribute(const EventPtr& event);

	std::vector<StateChangeListener*> listeners; // unordered
	StateChangeRecorder* recorder;
	bool viewOnlyMode;
};

} // namespace openmsx

#endif
