// $Id$

#ifndef STATECHANGEDISTRIBUTOR_HH
#define STATECHANGEDISTRIBUTOR_HH

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

	/** Unregisters a previously registered  listener.
	 * @param listener Listener to unregister.
	 */
	void unregisterListener(StateChangeListener& listener);

	/** Deliver the event to all registered listeners
	  * @param event The event
	  */
	void distribute(EventPtr event);

private:
	bool isRegistered(StateChangeListener* listener) const;

	typedef std::vector<StateChangeListener*> Listeners;
	Listeners listeners;
};

} // namespace openmsx

#endif
