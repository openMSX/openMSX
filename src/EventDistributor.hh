// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <map>
#include <queue>
#include "Thread.hh"
#include "Mutex.hh"


class EventListener
{
	public:
		/**
		 * This method gets called when an event you are interested in
		 * occurs. 
		 * Note: asynchronous events are deliverd in a different thread!!
		 */
		virtual void signalEvent(SDL_Event &event) = 0;
};

class EventDistributor : public Runnable
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
		 * Delivering of events must be triggerd by calling the method
		 * "pollSyncEvents()". This will deliver all synchronous events,
		 * not just the ones registerd by your class.
		 * The events are deliverd in a synchronous manner, that is by
		 * the same thread that executed the "pollSyncEvents()" method.
		 * 
		 * This method may not be call called from within a
		 * "signalEvent()" method
		 */
		void registerSyncListener (int type, EventListener *listener);
		
		/** 
		 * See "registerSyncListener()"
		 * 
		 * This method may not be call called from within a
		 * "signalEvent()" method
		 */
		void pollSyncEvents();
		
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

	private:
		EventDistributor();
		static EventDistributor *oneInstance;

		std::multimap <int, EventListener*> asyncMap;
		std::multimap <int, EventListener*> syncMap;
		std::queue <std::pair<SDL_Event, EventListener*> > queue;
		Mutex asyncMutex;	// to lock variable asyncMap
		Mutex syncMutex;	// to lock variable syncMap
		Mutex queueMutex;	// to lock variable queue
};

#endif
