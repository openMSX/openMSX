#ifndef STATECHANGEDISTRIBUTOR_HH
#define STATECHANGEDISTRIBUTOR_HH

#include "ReverseManager.hh"
#include "StateChangeListener.hh"
#include "EmuTime.hh"
#include <memory>
#include <utility>
#include <vector>

namespace openmsx {

class StateChangeDistributor
{
public:
	StateChangeDistributor() = default;
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
	void registerRecorder  (ReverseManager& recorder);
	void unregisterRecorder(ReverseManager& recorder);

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
	template<typename T, typename... Args>
	void distributeNew(EmuTime::param time, Args&& ...args) {
		if (recorder) {
			if (isReplaying()) {
				if (viewOnlyMode) return;
				stopReplay(time);
			}
			assert(!isReplaying());
			const auto& event = recorder->record<T>(time, std::forward<Args>(args)...);
			distribute(event); // might throw, ok
		} else {
			StateChange event(std::in_place_type_t<T>{},
			                  time, std::forward<Args>(args)...);
			distribute(event); // might throw, ok
		}
	}

	void distributeReplay(const StateChange& event) {
		assert(isReplaying());
		distribute(event);
	}

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
	[[nodiscard]] bool isViewOnlyMode() const { return viewOnlyMode; }

	[[nodiscard]] bool isReplaying() const;

private:
	[[nodiscard]] bool isRegistered(StateChangeListener* listener) const;
	void distribute(const StateChange& event);

private:
	std::vector<StateChangeListener*> listeners; // unordered
	ReverseManager* recorder = nullptr;
	bool viewOnlyMode = false;
};

} // namespace openmsx

#endif
