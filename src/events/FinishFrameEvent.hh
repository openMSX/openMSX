// $Id$

#ifndef __FINISHFRAMEEVENT_HH__
#define __FINISHFRAMEEVENT_HH__

#include "Event.hh"
#include "VideoSourceSetting.hh"

namespace openmsx {

class FinishFrameEvent: public Event
{
public:
	FinishFrameEvent(VideoSource source_)
		: Event(FINISH_FRAME_EVENT)
		, source(source_)
	{
		// nothing
	}

	VideoSource getSource() const { return source; }

private:
	VideoSource source;
};

}
#endif // __FINISHFRAMEEVENT_HH__
