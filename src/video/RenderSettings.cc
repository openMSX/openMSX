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
	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy.reset(new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlace.reset(new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkip.reset(new IntegerSetting(
		"maxFrameSkip", "set the max amount of frameskip", 3, 0, 100));

	minFrameSkip.reset(new IntegerSetting(
		"minframeskip", "set the min amount of frameskip", 0, 0, 100));

	fullScreen.reset(new BooleanSetting(
		"fullscreen", "full screen display on/off", false));

	gamma.reset(new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	glow.reset(new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	horizontalBlur.reset(new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	// Get user-preferred renderer from config.
	renderer = RendererFactory::createRendererSetting();
	currentRenderer = renderer->getValue();

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scalerMap["blur"] = SCALER_BLUR;
	scaler.reset(new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap));

	scanlineAlpha.reset(new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));

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
