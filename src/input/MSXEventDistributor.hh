// $Id$

#ifndef MSXEVENTDISTRIBUTOR_HH
#define MSXEVENTDISTRIBUTOR_HH

#include "shared_ptr.hh"
#include <vector>

namespace openmsx {

class MSXEventListener;
class Event;
class EmuTime;

class MSXEventDistributor
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

	void distributeEvent(EventPtr event, const EmuTime& time);

private:
	typedef std::vector<MSXEventListener*> Listeners;
	Listeners listeners;
};

} // namespace openmsx

#endif
