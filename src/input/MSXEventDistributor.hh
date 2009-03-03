// $Id$

#ifndef MSXEVENTDISTRIBUTOR_HH
#define MSXEVENTDISTRIBUTOR_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include "shared_ptr.hh"
#include <vector>

namespace openmsx {

class MSXEventListener;
class Event;

class MSXEventDistributor : private noncopyable
{
public:
	typedef shared_ptr<const Event> EventPtr;

	MSXEventDistributor();
	~MSXEventDistributor();

	/**
	 * Registers a given object to receive certain events.
	 * @param listener Listener that will be notified when an event arrives.
	 */
	void registerEventListener(MSXEventListener& listener);

	/**
	 * Unregisters a previously registered event listener.
	 * @param listener Listener to unregister.
	 */
	void unregisterEventListener(MSXEventListener& listener);

	/** Deliver the event to all registered listeners
	  * @param event The event
	  * @param time Current time
	  * Note: MSXEventListener's are allowed to throw exceptions, and this
	  *       method doesn't catch them (in case of an exception it's
	  *       undefined which listeners receive the event)
	  */
	void distributeEvent(EventPtr event, EmuTime::param time);

private:
	bool isRegistered(MSXEventListener* listener) const;

	typedef std::vector<MSXEventListener*> Listeners;
	Listeners listeners;
};

} // namespace openmsx

#endif
