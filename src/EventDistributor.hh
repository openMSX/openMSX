// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <map>
#include <queue>
#include "Thread.hh"
#include "Mutex.hh"
#include "Schedulable.hh"

// forward declaration
class EventListener;


class EventDistributor : public Runnable , private Schedulable
{
	public:
		virtual ~EventDistributor();
		static EventDistributor *instance();

		void run();

		/**
		 * Use this method to register a given class to synchronously
		 * receive certain SDL_Event's. When such an event is received
		 * it will 'eventually' be passed to the registerd class its 
		 * "signalEvent()" method.
		 * The 'main-emulation-thread' will deliver the event.
		 * 
		 * This method may not be call called from within a
		 * "signalEvent()" method
		 */
		void registerSyncListener (int type, EventListener *listener);
		
		/**
		 * Use this method to register a given class to asynchronously
		 * receive certain SDL_Event's. When such an event is received
		 * it will almost immediately be passed to the registerd class
		 * its "signalEvent()" method.
		 * The events are deliverd in an asynchronous manner, that is
		 * by differnt thread as the 'main-emulation-thread', so take
		 * care of locking issues.
		 *
		 * This method may not be call called from within a
		 * "signalEvent()" method
		 */
		void registerAsyncListener(int type, EventListener *listener);

		enum handled {};	// to throw

	private:
		EventDistributor();
		static EventDistributor *oneInstance;
		void executeUntilEmuTime(const EmuTime &time, int userdata);

		std::multimap <int, EventListener*> asyncMap;
		std::multimap <int, EventListener*> syncMap;
		std::queue <std::pair<SDL_Event, EventListener*> > queue;
		Mutex asyncMutex;	// to lock variable asyncMap
		Mutex syncMutex;	// to lock variable syncMap
		Mutex queueMutex;	// to lock variable queue
};

#endif
