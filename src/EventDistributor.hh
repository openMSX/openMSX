
#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <SDL/SDL.h>
#include <stl.h>


class EventListener
{
	public:
		virtual void signalEvent(SDL_Event &event) = 0;
};

class EventDistributor 
{
	public:
		~EventDistributor();
		static EventDistributor *instance();

		void run();
		void registerListener(int type, EventListener *listener);

	private:
		EventDistributor();
		static EventDistributor *oneInstance;

		SDL_Event event;
		std::multimap <int, EventListener*> map;
};

#endif
