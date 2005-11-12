// $Id$

#ifndef FINISHFRAMEEVENT_HH
#define FINISHFRAMEEVENT_HH

#include "Event.hh"
#include "VideoSourceSetting.hh"

namespace openmsx {

class FinishFrameEvent: public Event
{
public:
	explicit FinishFrameEvent(VideoSource source_)
		: Event(OPENMSX_FINISH_FRAME_EVENT)
		, source(source_)
	{
		// nothing
	}

	VideoSource getSource() const { return source; }

private:
	VideoSource source;
};

}

#endif
