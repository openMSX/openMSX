// $Id$

#ifndef __FINISHFRAMEEVENT_HH__
#define __FINISHFRAMEEVENT_HH__

#include "Event.hh"
#include "RenderSettings.hh"

namespace openmsx {

class FinishFrameEvent: public Event
{
public:

	FinishFrameEvent(RenderSettings::VideoSource source_)
		: Event(FINISH_FRAME_EVENT)
		, source(source_)
	{
		// nothing
	}

	RenderSettings::VideoSource getSource() const { return source; }

private:
	RenderSettings::VideoSource source;
};

}
#endif // __FINISHFRAMEEVENT_HH__
