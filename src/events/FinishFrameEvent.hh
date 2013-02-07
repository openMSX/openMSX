// $Id$

#ifndef FINISHFRAMEEVENT_HH
#define FINISHFRAMEEVENT_HH

#include "Event.hh"
#include "VideoSource.hh"
#include "TclObject.hh"
#include "checked_cast.hh"

namespace openmsx {

class FinishFrameEvent : public Event
{
public:
	FinishFrameEvent(VideoSource source_, bool skipped_)
		: Event(OPENMSX_FINISH_FRAME_EVENT)
		, source(source_), skipped(skipped_)
	{
	}

	VideoSource getSource() const { return source; }
	bool isSkipped() const { return skipped; }

	virtual void toStringImpl(TclObject& result) const
	{
		result.addListElement("finishframe");
		result.addListElement(int(source));
		result.addListElement(skipped);
	}
	virtual bool lessImpl(const Event& other) const
	{
		auto& ffEv = checked_cast<const FinishFrameEvent&>(other);
		return (getSource() != ffEv.getSource())
		     ? (getSource() <  ffEv.getSource())
		     : (isSkipped() <  ffEv.isSkipped());

	}

private:
	const VideoSource source;
	const bool skipped;
};

} // namespace openmsx

#endif
