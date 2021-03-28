#ifndef FINISHFRAMEEVENT_HH
#define FINISHFRAMEEVENT_HH

#include "Event.hh"
#include "TclObject.hh"
#include "checked_cast.hh"
#include <tuple>

namespace openmsx {

/**
 * This event is send when a device (v99x8, v9990, video9000, laserdisc)
 * reaches the end of a frame. This event has info on:
 *  - which device generated the event
 *  - which video layer was active
 *  - was the frame actually rendered or not (frameskip)
 * Note that even if a frame was rendered (not skipped) it may not (need to) be
 * displayed because the corresponding video layer is not active. Or also even
 * if the corresponding video layer for a device is not active, the rendered
 * frame may still be displayed as part of a superimposed video layer.
 */
class FinishFrameEvent final : public Event
{
public:
	FinishFrameEvent(int thisSource_, int selectedSource_,
	                 bool skipped_)
		: Event(OPENMSX_FINISH_FRAME_EVENT)
		, thisSource(thisSource_), selectedSource(selectedSource_)
		, skipped(skipped_)
	{
	}

	[[nodiscard]] int getSource()         const { return thisSource; }
	[[nodiscard]] int getSelectedSource() const { return selectedSource; }
	[[nodiscard]] bool isSkipped() const { return skipped; }
	[[nodiscard]] bool needRender() const { return !skipped && (thisSource == selectedSource); }

	[[nodiscard]] TclObject toTclList() const override
	{
		return makeTclList("finishframe",
		                   int(thisSource),
		                   int(selectedSource),
		                   skipped);
	}
	[[nodiscard]] bool equalImpl(const Event& other) const override
	{
		const auto& e = checked_cast<const FinishFrameEvent&>(other);
		return std::tuple(  getSource(),   getSelectedSource(),   isSkipped()) ==
		       std::tuple(e.getSource(), e.getSelectedSource(), e.isSkipped());
	}

private:
	const int thisSource;
	const int selectedSource;
	const bool skipped;
};

} // namespace openmsx

#endif
