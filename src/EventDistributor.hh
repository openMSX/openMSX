
#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <multimap.h>


class EventListener
{
	public:
		// note: this method runs in a different thread!!
		virtual void signalEvent(SDL_Event &event) = 0;
};

class EventDistributor : public EventListener
{
	public:
		virtual ~EventDistributor();
		static EventDistributor *instance();

		void run();

		// this method may not be called from within a signalEvent() method
		void registerListener(int type, EventListener *listener);

		// EventListener
		void signalEvent(SDL_Event &event);

	private:
		EventDistributor();
		static EventDistributor *oneInstance;

		SDL_Event event;
		std::multimap <int, EventListener*> map;
		SDL_mutex *mut;	// to lock variable map
};

#endif
