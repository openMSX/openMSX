// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <map>
#include <queue>
#include "Thread.hh"
#include "Mutex.hh"
#include "Schedulable.hh"
#include "Command.hh"

// forward declaration
class EventListener;


class EventDistributor : public Runnable , private Schedulable
{
	public:
		virtual ~EventDistributor();
		static EventDistributor *instance();

		/**
		 * Use this method to (un)register a given class to receive
		 * certain SDL_Event's. When such an event is received it
		 * will 'eventually' be passed to the registerd class its 
		 * "signalEvent()" method.
		 * The 'main-emulation-thread' will deliver the event.
		 */
		void   registerEventListener(int type, EventListener *listener);
		void unregisterEventListener(int type, EventListener *listener);
		
	private:
		EventDistributor();
		virtual void executeUntilEmuTime(const EmuTime &time, int userdata);
		virtual void run();

		static EventDistributor *oneInstance;
		
		std::multimap <int, EventListener*> syncMap;
		std::queue <std::pair<SDL_Event, EventListener*> > queue;
		Mutex syncMutex;	// to lock variable syncMap
		Mutex queueMutex;	// to lock variable queue

		class BindCmd : public Command {
			public:
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help(const std::vector<std::string> &tokens);
		};
		BindCmd bindCmd;
		
		class UnbindCmd : public Command {
			public:
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help(const std::vector<std::string> &tokens);
		};
		UnbindCmd unbindCmd;
};

#endif
