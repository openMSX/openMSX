// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <map>
#include <queue>
#include "Schedulable.hh"
#include "Command.hh"
#include "Settings.hh"

using std::map;
using std::queue;
using std::pair;

namespace openmsx {

class EventListener;


class EventDistributor : private SettingListener
{
	public:
		virtual ~EventDistributor();
		static EventDistributor *instance();

		/** This is the main loop. It waits for events.
		  * It does not return until openMSX is quit.
		  * This method runs in the main thread.
		  */
		virtual void run();

		/**
		 * Use this method to (un)register a given class to receive
		 * certain SDLEvents.
		 * @param type The SDLEventType number of the events you want to receive
		 * @param listener Object that will be notified when the events arrives
		 * @param priority The priority of the listener (lower number is higher
		 *        priority). Higher priority listeners may block an event for
		 *        lower priority listeners. Normally you don't need to specify
		 *        a priority.
		 *        Note: in the current implementation there are only two
		 *              priority levels (0 and !=0)
		 * The delivery of the event is done by the 'main-emulation-thread',
		 * so there is no need for extra synchronization.
		 */
		void   registerEventListener(int type, EventListener *listener, int priority = 0);
		void unregisterEventListener(int type, EventListener *listener, int priority = 0);

	private:
		EventDistributor();

		/** Passes an event to the emulation thread.
		  */
		void handleInEmu(SDL_Event &event);

		void update(const SettingLeafNode *setting);

		multimap <int, EventListener*> lowMap;
		multimap <int, EventListener*> highMap;
		queue <pair<SDL_Event, EventListener*> > lowQueue;
		queue <pair<SDL_Event, EventListener*> > highQueue;
		BooleanSetting grabInput;

		/** Quit openMSX.
		  * Starts the shutdown procedure.
		  */
		void quit();

		class QuitCommand : public Command {
		public:
			virtual string execute(const vector<string> &tokens)
				throw ();
			virtual string help(const vector<string> &tokens) const;
		} quitCommand;
		friend class QuitCommand;
};

} // namespace openmsx

#endif
