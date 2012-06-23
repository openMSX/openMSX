// $Id$

#include "Video9000.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include "checked_cast.hh"
#include "serialize.hh"

namespace openmsx {

Video9000::Video9000(const DeviceConfig& config)
	: MSXDevice(config)
	, VideoLayer(getMotherBoard(), VIDEO_9000)
	, display(getReactor().getDisplay())
{
	EventDistributor& distributor = getReactor().getEventDistributor();
	distributor.registerEventListener(OPENMSX_FINISH_FRAME_EVENT, *this);
	display.attach(*this);

	activeLayer = NULL; // we can't set activeLayer yet
	value = 0x10;
}

Video9000::~Video9000()
{
	EventDistributor& distributor = getReactor().getEventDistributor();
	distributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT, *this);
	display.detach(*this);
}

void Video9000::reset(EmuTime::param time)
{
	Video9000::writeIO(0x6f, 0x10, time);
}

void Video9000::writeIO(word /*port*/, byte newValue, EmuTime::param /*time*/)
{
	if (newValue == value) return;
	value = newValue;
	recalc();
}

void Video9000::recalc()
{
	// TODO can we do better than name based lookup?
	VideoLayer* v99x8Layer = dynamic_cast<VideoLayer*>(display.findLayer("V99x8"));
	VideoLayer* v9990Layer = dynamic_cast<VideoLayer*>(display.findLayer("V9990"));
	// TODO Is it required to check for NULL?
	//      Can only happen when you use a Video9000 when either
	//      V99x8 or V9990 is missing in the MSX machine.

	// ...0.... -> only V9990
	// ...10... -> only V99x8
	// ...11... -> superimpose   TODO not yet implemented
	bool showV9990 = ((value & 0x18) != 0x10);
	if (showV9990) {
		if (v99x8Layer) v99x8Layer->setVideo9000Active(false);
		if (v9990Layer) v9990Layer->setVideo9000Active(true);
		activeLayer = v9990Layer;
	} else {
		if (v99x8Layer) v99x8Layer->setVideo9000Active(true);
		if (v9990Layer) v9990Layer->setVideo9000Active(false);
		activeLayer = v99x8Layer;
	}
	if (!activeLayer) {
		// only happens on a MSX system with Video9000 but with
		// missing V99x8 or V9990
		activeLayer = display.findLayer("snow");
	}
}

void Video9000::preVideoSystemChange()
{
	activeLayer = NULL; // will be recalculated on next paint()
}

void Video9000::postVideoSystemChange()
{
	// We can't yet re-initialize 'activeLayer' here because the
	// new v99x8/v9990 layer may not be created yet.
}

void Video9000::paint(OutputSurface& output)
{
	if (!activeLayer) {
		recalc();
	}
	activeLayer->paint(output);
}

string_ref Video9000::getLayerName() const
{
	return "Video9000";
}

int Video9000::signalEvent(const shared_ptr<const Event>& event)
{
	assert(event->getType() == OPENMSX_FINISH_FRAME_EVENT);
	const FinishFrameEvent& ffe =
			checked_cast<const FinishFrameEvent&>(*event);
	if (ffe.isSkipped()) return 0;
	if (display.getRenderSettings().getVideoSource().getValue() != VIDEO_9000) return 0;
	bool showV9990 = ((value & 0x18) != 0x10);
	if (( showV9990 && (ffe.getSource() == VIDEO_GFX9000)) ||
	    (!showV9990 && (ffe.getSource() == VIDEO_MSX))) {
		getReactor().getEventDistributor().distributeEvent(
			new FinishFrameEvent(VIDEO_9000, false));
	}
	return 0;
}

template<typename Archive>
void Video9000::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("value", value);
}
INSTANTIATE_SERIALIZE_METHODS(Video9000);
REGISTER_MSXDEVICE(Video9000, "Video9000");

} // namespace openmsx
