// $Id$

#include "UserEvents.hh"
#include "openmsx.hh"
#include "RendererFactory.hh"
#include "Scheduler.hh"

void UserEvents::push(
	UserEvents::UserEventID id, void *data1, void *data2 )
{
	// Push user event into SDL event queue.
	// SDL_PeepEvents operates on a copy of the event, so we can use a temp.
	SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.code = id;
	event.user.data1 = data1;
	event.user.data2 = data2;
	// We cannot use SDL_PushEvent, because it is incompatible:
	// SDL 1.2.5 returns 0 on OK, -1 on error.
	// SDL 1.2.4 returns 1 on OK, 0 on queue full and -1 on other errors.
	while (SDL_PeepEvents(&event, 1, SDL_ADDEVENT, 0) < 1) {
		SDL_Delay(100); // retry in 100 ms
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

