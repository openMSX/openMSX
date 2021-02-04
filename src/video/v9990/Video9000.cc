#include "Video9000.hh"
#include "VDP.hh"
#include "V9990.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "PostProcessor.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "MSXMotherBoard.hh"
#include "VideoSourceSetting.hh"
#include "CommandException.hh"
#include "checked_cast.hh"
#include "serialize.hh"

namespace openmsx {

Video9000::Video9000(const DeviceConfig& config)
	: MSXDevice(config)
	, VideoLayer(getMotherBoard(), getName())
	, videoSourceSetting(getMotherBoard().getVideoSource())
{
	EventDistributor& distributor = getReactor().getEventDistributor();
	distributor.registerEventListener(OPENMSX_FINISH_FRAME_EVENT, *this);
	getReactor().getDisplay().attach(*this);

	activeLayer = nullptr; // we can't set activeLayer yet
	v99x8Layer = nullptr;
	v9990Layer = nullptr;
	value = 0x10;
}

void Video9000::init()
{
	MSXDevice::init();
	auto refs = getReferences(); // make copy
	bool error = false;
	if (refs.size() != 2) error = true;
	if (!error && !dynamic_cast<VDP*>(refs[0])) {
		std::swap(refs[0], refs[1]); // try reverse order
	}
	if (!error) vdp   = dynamic_cast<VDP*  >(refs[0]);
	if (!error) v9990 = dynamic_cast<V9990*>(refs[1]);
	if (error || !vdp || !v9990) {
		throw MSXException(
			"Invalid Video9000 configuration: "
			"need reference to VDP and V9990 device.");
	}
}

Video9000::~Video9000()
{
	EventDistributor& distributor = getReactor().getEventDistributor();
	distributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT, *this);
	getReactor().getDisplay().detach(*this);
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
	int video9000id = getVideoSource();
	v99x8Layer = vdp  ->getPostProcessor();
	v9990Layer = v9990->getPostProcessor();
	assert(!!v99x8Layer == !!v9990Layer); // either both or neither

	// ...0.... -> only V9990
	// ...10... -> only V99x8
	// ...11... -> superimpose
	bool showV99x8   = ((value & 0x10) == 0x10);
	bool showV9990   = ((value & 0x18) != 0x10);
	assert(showV99x8 || showV9990);
	if (v99x8Layer) v99x8Layer->setVideo9000Active(
		video9000id,
		showV99x8 ? (showV9990 ? VideoLayer::ACTIVE_BACK
		                       : VideoLayer::ACTIVE_FRONT)
		          : VideoLayer::INACTIVE);
	if (v9990Layer) v9990Layer->setVideo9000Active(
		video9000id,
		showV9990 ? VideoLayer::ACTIVE_FRONT : VideoLayer::INACTIVE);
	activeLayer = showV9990 ? v9990Layer : v99x8Layer;
	// activeLayer==nullptr is possible for renderer=none
	recalcVideoSource();
}

void Video9000::recalcVideoSource()
{
	// Disable superimpose when gfx9000 layer is selected. That way you
	// can look at the gfx9000-only output even when the video9000 software
	// enabled superimpose mode (mostly useful for debugging).
	bool superimpose = ((value & 0x18) == 0x18);
	v9990->setExternalVideoSource(
		superimpose && (videoSourceSetting.getSource() == getVideoSource()));
}

void Video9000::preVideoSystemChange() noexcept
{
	activeLayer = nullptr; // will be recalculated on next paint()
}

void Video9000::postVideoSystemChange() noexcept
{
	// We can't yet re-initialize 'activeLayer' here because the
	// new v99x8/v9990 layer may not be created yet.
}

void Video9000::paint(OutputSurface& output)
{
	if (!activeLayer) {
		recalc();
	}
	// activeLayer==nullptr is possible for renderer=none, but in that case
	// the paint() method will never be called.
	activeLayer->paint(output);
}

void Video9000::takeRawScreenShot(unsigned height, const std::string& filename)
{
	auto* layer = dynamic_cast<VideoLayer*>(activeLayer);
	if (!layer) {
		throw CommandException("TODO");
	}
	layer->takeRawScreenShot(height, filename);
}

int Video9000::signalEvent(const std::shared_ptr<const Event>& event) noexcept
{
	int video9000id = getVideoSource();

	assert(event->getType() == OPENMSX_FINISH_FRAME_EVENT);
	const auto& ffe = checked_cast<const FinishFrameEvent&>(*event);
	if (ffe.isSkipped()) return 0;
	if (videoSourceSetting.getSource() != video9000id) return 0;

	bool superimpose = ((value & 0x18) == 0x18);
	if (superimpose && v99x8Layer && v9990Layer &&
	    (ffe.getSource() == v99x8Layer->getVideoSource())) {
		// inform V9990 about the new V99x8 frame
		v9990Layer->setSuperimposeVdpFrame(v99x8Layer->getPaintFrame());
	}

	bool showV9990   = ((value & 0x18) != 0x10); // v9990 or superimpose
	if (( showV9990 && v9990Layer && (ffe.getSource() == v9990Layer->getVideoSource())) ||
	    (!showV9990 && v99x8Layer && (ffe.getSource() == v99x8Layer->getVideoSource()))) {
		getReactor().getEventDistributor().distributeEvent(
			std::make_shared<FinishFrameEvent>(
				video9000id, video9000id, false));
	}
	return 0;
}

void Video9000::update(const Setting& setting) noexcept
{
	VideoLayer::update(setting);
	if (&setting == &videoSourceSetting) {
		recalcVideoSource();
	}
}

template<typename Archive>
void Video9000::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("value", value);
	if (ar.isLoader()) {
		recalc();
	}
}
INSTANTIATE_SERIALIZE_METHODS(Video9000);
REGISTER_MSXDEVICE(Video9000, "Video9000");

} // namespace openmsx
