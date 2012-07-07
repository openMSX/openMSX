// $Id$

#include "Video9000.hh"
#include "V9990.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "PostProcessor.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include "CommandException.hh"
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

void Video9000::init()
{
	MSXDevice::init();
	const MSXDevice::Devices& references = getReferences();
	v9990 = references.empty()
	      ? NULL
	      : dynamic_cast<V9990*>(references[0]);
	if (!v9990) {
		throw MSXException("Invalid Video9000 configuration: "
		                   "need reference to V9990 device.");
	}
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
	v99x8Layer = dynamic_cast<PostProcessor*>(display.findLayer("V99x8"));
	v9990Layer = dynamic_cast<PostProcessor*>(display.findLayer("V9990"));

	// ...0.... -> only V9990
	// ...10... -> only V99x8
	// ...11... -> superimpose
	bool showV99x8   = ((value & 0x10) == 0x10);
	bool showV9990   = ((value & 0x18) != 0x10);
	assert(showV99x8 || showV9990);
	if (v99x8Layer) v99x8Layer->setVideo9000Active(
		showV99x8 ? (showV9990 ? VideoLayer::ACTIVE_BACK
		                       : VideoLayer::ACTIVE_FRONT)
		          : VideoLayer::INACTIVE);
	if (v9990Layer) v9990Layer->setVideo9000Active(
		showV9990 ? VideoLayer::ACTIVE_FRONT : VideoLayer::INACTIVE);
	activeLayer = showV9990 ? v9990Layer : v99x8Layer;
	if (!activeLayer) {
		// only happens on a MSX system with Video9000 but with
		// missing V99x8 or V9990
		activeLayer = display.findLayer("snow");
	}
	recalcVideoSource();
}

void Video9000::recalcVideoSource()
{
	// Disable superimpose when gfx9000 layer is selected. That way you
	// can look at the gfx9000-only output even when the video9000 software
	// enabled superimpose mode (mostly useful for debugging).
	VideoSourceSetting& videoSource = display.getRenderSettings().getVideoSource();
	bool superimpose = ((value & 0x18) == 0x18);
	v9990->setExternalVideoSource(superimpose &&
	                              (videoSource.getValue() == VIDEO_9000));
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

void Video9000::takeRawScreenShot(unsigned height, const std::string& filename)
{
	VideoLayer* layer = dynamic_cast<VideoLayer*>(activeLayer);
	if (!layer) {
		throw CommandException("TODO");
	}
	layer->takeRawScreenShot(height, filename);
}

int Video9000::signalEvent(const shared_ptr<const Event>& event)
{
	assert(event->getType() == OPENMSX_FINISH_FRAME_EVENT);
	const FinishFrameEvent& ffe =
			checked_cast<const FinishFrameEvent&>(*event);
	if (ffe.isSkipped()) return 0;
	if (display.getRenderSettings().getVideoSource().getValue() != VIDEO_9000) return 0;

	bool superimpose = ((value & 0x18) == 0x18);
	if (superimpose && (ffe.getSource() == VIDEO_MSX) &&
	    v99x8Layer && v9990Layer) {
		// inform V9990 about the new V99x8 frame
		v9990Layer->setSuperimposeVdpFrame(v99x8Layer->getPaintFrame());
	}

	bool showV9990   = ((value & 0x18) != 0x10); // v9990 or superimpose
	if (( showV9990 && (ffe.getSource() == VIDEO_GFX9000)) ||
	    (!showV9990 && (ffe.getSource() == VIDEO_MSX))) {
		getReactor().getEventDistributor().distributeEvent(
			new FinishFrameEvent(VIDEO_9000, false));
	}
	return 0;
}

void Video9000::update(const Setting& setting)
{
	VideoLayer::update(setting);
	if (&setting == &display.getRenderSettings().getVideoSource()) {
		recalcVideoSource();
	}
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
