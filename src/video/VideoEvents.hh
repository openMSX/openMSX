// $Id$

#ifndef __INPUTEVENTS_HH__
#define __INPUTEVENTS_HH__

#include "Event.hh"

namespace openmsx {

class FinishFrameEvent : public Event
{
public:
	FinishFrameEvent() : Event(FINISH_FRAME_EVENT) {}
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
