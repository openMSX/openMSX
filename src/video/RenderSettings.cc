// $Id$

#include "RenderSettings.hh"
#include "SettingsConfig.hh"
#include "CliCommOutput.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "VDP.hh"
#include "Event.hh"
#include "EventDistributor.hh"


namespace openmsx {

RenderSettings::RenderSettings()
{
	XMLElement& config =
		SettingsConfig::instance().getCreateChild("video");

	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	XMLElement& accElem = config.getCreateChild("accuracy", "pixel");
	Accuracy accValue = ACC_PIXEL;
	EnumSetting<Accuracy>::Map::const_iterator accIt =
		accMap.find(accElem.getData());
	if (accIt != accMap.end()) {
		accValue = accIt->second;
	}
	accuracy.reset(new EnumSetting<Accuracy>(
		accElem, "rendering accuracy", accValue, accMap));

	XMLElement& deintElem = config.getCreateChild("deinterlace", "on");
	deinterlace.reset(new BooleanSetting(
		deintElem, "deinterlacing on/off"));

	XMLElement& maxSkipElem = config.getCreateChild("maxframeskip", "3");
	maxFrameSkip.reset(new IntegerSetting(
		maxSkipElem, "set the max amount of frameskip", 0, 100));

	XMLElement& minSkipElem = config.getCreateChild("minframeskip", "0");
	minFrameSkip.reset(new IntegerSetting(
		minSkipElem, "set the min amount of frameskip", 0, 100));

	XMLElement& fsElem = config.getCreateChild("fullscreen", "false");
	fullScreen.reset(new BooleanSetting(fsElem,
		"full screen display on/off"));

	XMLElement& gammaElem = config.getCreateChild("gamma", "1.1");
	gamma.reset(new FloatSetting(
		gammaElem, "amount of gamma correction: low is dark, high is bright",
		0.1, 5.0));

	XMLElement& glowElem = config.getCreateChild("glow", "0");
	glow.reset(new IntegerSetting(
		glowElem, "amount of afterglow effect: 0 = none, 100 = lots",
		0, 100));

	XMLElement& blurElem = config.getCreateChild("blur", "50");
	horizontalBlur.reset(new IntegerSetting(
		blurElem, "amount of horizontal blur effect: 0 = none, 100 = full",
		0, 100));

	// Get user-preferred renderer from config.
	XMLElement& rendererElem = config.getCreateChild("renderer", "SDLHi");
	renderer = RendererFactory::createRendererSetting(rendererElem);
	currentRenderer = renderer->getValue();

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scalerMap["blur"] = SCALER_BLUR;
	XMLElement& scalerElem = config.getCreateChild("scaler", "simple");
	ScalerID scalerValue = SCALER_SIMPLE;
	EnumSetting<ScalerID>::Map::const_iterator scalerIt =
		scalerMap.find(scalerElem.getData());
	if (scalerIt != scalerMap.end()) {
		scalerValue = scalerIt->second;
	}
	scaler.reset(new EnumSetting<ScalerID>(
		scalerElem, "scaler algorithm", scalerValue, scalerMap));

	XMLElement& scanElem = config.getCreateChild("scanline", "20");
	scanlineAlpha.reset(new IntegerSetting(
		scanElem, "amount of scanline effect: 0 = none, 100 = full", 0, 100
		));

	renderer->addListener(this);
	fullScreen->addListener(this);
	EventDistributor::instance().registerEventListener(
		RENDERER_SWITCH_EVENT, *this, EventDistributor::EMU );
}

RenderSettings::~RenderSettings()
{
	EventDistributor::instance().unregisterEventListener(
		RENDERER_SWITCH_EVENT, *this, EventDistributor::EMU );
	fullScreen->removeListener(this);
	renderer->removeListener(this);
}

RenderSettings& RenderSettings::instance()
{
	static RenderSettings oneInstance;
	return oneInstance;
}

void RenderSettings::update(const SettingLeafNode* setting)
{
	if (setting == renderer.get()) {
		checkRendererSwitch();
	} else if (setting == fullScreen.get()) {
		checkRendererSwitch();
	} else {
		assert(false);
	}
}

void RenderSettings::checkRendererSwitch()
{
	// Tell renderer to sync with render settings.
	if (renderer->getValue() != currentRenderer
	|| !Display::INSTANCE->getVideoSystem()->checkSettings() ) {
		currentRenderer = renderer->getValue();
		// Renderer failed to sync; replace it.
		Event* rendererSwitchEvent = new SimpleEvent<RENDERER_SWITCH_EVENT>();
		EventDistributor::instance().distributeEvent(rendererSwitchEvent);
	}
}

bool RenderSettings::signalEvent(const Event& event)
{
	assert(event.getType() == RENDERER_SWITCH_EVENT);
	// TODO: Switch video system here, not in RendererFactory.
	VDP::instance->switchRenderer();
	return true;
}

} // namespace openmsx
