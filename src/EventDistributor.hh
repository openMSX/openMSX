
#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <multimap.h>


class EventListener
{
	public:
		virtual void signalEvent(SDL_Event &event) = 0;
};

class EventDistributor : public EventListener
{
	public:
		virtual ~EventDistributor();
		static EventDistributor *instance();

		void run();
		void registerListener(int type, EventListener *listener);

		// EventListener
		void signalEvent(SDL_Event &event);

	private:
		EventDistributor();
		static EventDistributor *oneInstance;

		SDL_Event event;
		std::multimap <int, EventListener*> map;
};

#endif
