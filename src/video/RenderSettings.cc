// $Id$

#include "RenderSettings.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "CliCommOutput.hh"
#include "InfoCommand.hh"
#include "CommandResult.hh"

namespace openmsx {

RenderSettings::RenderSettings()
	: rendererInfo(*this),
	  scalerInfo(*this),
	  accuracyInfo(*this),
	  settingsConfig(SettingsConfig::instance()),
	  infoCommand(InfoCommand::instance())
{
	Config* config = settingsConfig.getConfigById("renderer");

	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy = new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_LINE, accMap);

	deinterlace = new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true);

	frameSkip = new FrameSkipSetting();

	bool fsBool = config->getParameterAsBool("full_screen", false);
	fullScreen = new BooleanSetting(
		"fullscreen", "full screen display on/off", fsBool);

	gamma = new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0);

	glow = new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100);

	horizontalBlur = new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100);

	// Get user-preferred renderer from config.
	string rendererName = config->getType();
	renderer = RendererFactory::createRendererSetting(rendererName);

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scaler = new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap);

	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);

	infoCommand.registerTopic("renderer", &rendererInfo);
	infoCommand.registerTopic("scaler", &scalerInfo);
	infoCommand.registerTopic("accuracy", &accuracyInfo);
}

RenderSettings::~RenderSettings()
{
	infoCommand.unregisterTopic("scaler", &scalerInfo);
	infoCommand.unregisterTopic("renderer", &rendererInfo);
	infoCommand.unregisterTopic("accuracy", &accuracyInfo);

	delete accuracy;
	delete deinterlace;
	delete horizontalBlur;
	delete scanlineAlpha;
	delete frameSkip;
}

RenderSettings& RenderSettings::instance()
{
	static RenderSettings oneInstance;
	return oneInstance;
}

// Renderer info

RenderSettings::RendererInfo::RendererInfo(RenderSettings& parent_)
	: parent(parent_)
{
}

void RenderSettings::RendererInfo::execute(const vector<string>& tokens,
	CommandResult& result) const throw()
{
	set<string> renderers;
	parent.getRenderer()->getPossibleValues(renderers);
	for (set<string>::const_iterator it = renderers.begin();
	     it != renderers.end(); ++it) {
		result.addListElement(*it);
	}
}

string RenderSettings::RendererInfo::help(const vector<string>& tokens) const
	throw()
{
	return "Shows a list of available renderers.\n";
}

// Scaler info

RenderSettings::ScalerInfo::ScalerInfo(RenderSettings& parent_)
	: parent(parent_)
{
}

void RenderSettings::ScalerInfo::execute(const vector<string>& tokens,
	CommandResult& result) const throw()
{
	set<string> scalers;
	parent.getScaler()->getPossibleValues(scalers);
	for (set<string>::const_iterator it = scalers.begin();
	     it != scalers.end(); ++it) {
		result.addListElement(*it);
	}
}

string RenderSettings::ScalerInfo::help(const vector<string>& tokens) const
	throw()
{
	return "Shows a list of available scalers.\n";
}

// Accuracy info

RenderSettings::AccuracyInfo::AccuracyInfo(RenderSettings& parent_)
	: parent(parent_)
{
}

void RenderSettings::AccuracyInfo::execute(const vector<string>& tokens,
	CommandResult& result) const throw()
{
	set<string> accuracies;
	parent.getAccuracy()->getPossibleValues(accuracies);
	for (set<string>::const_iterator it = accuracies.begin();
	     it != accuracies.end(); ++it) {
		result.addListElement(*it);
	}
}

string RenderSettings::AccuracyInfo::help(const vector<string>& tokens) const
	throw()
{
	return "Shows a list of available accuracies.\n";
}

} // namespace openmsx
