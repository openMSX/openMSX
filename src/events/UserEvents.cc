// $Id$

#include "UserEvents.hh"
#include "openmsx.hh"
#include "RendererFactory.hh"
#include "Scheduler.hh"

void UserEvents::push(
	UserEvents::UserEventID id, void *data1, void *data2 )
{
	// Push user event into SDL event queue.
	// SDL_PushEvent operates on a copy of the event, so we can use a temp.
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = id;
	event.user.data1 = data1;
	event.user.data2 = data2;
	while (true) {
		int pushed = SDL_PushEvent(&event);
		if (pushed == 1) break;
		SDL_Delay(100);
	}
}

void UserEvents::handle(SDL_UserEvent &event) {
	assert(event.type == SDL_USEREVENT);
	UserEventID id = (UserEventID)event.code;
	switch (id) {
	case QUIT:
		Scheduler::instance()->stopScheduling();
		break;
	case RENDERER_SWITCH:
		((RendererSwitcher *)event.data1)->handleEvent();
		break;
	case FULL_SCREEN_TOGGLE:
		((FullScreenToggler *)event.data1)->handleEvent();
		break;
	default:
		PRT_ERROR("Cannot handle event " << id);
	}
}

