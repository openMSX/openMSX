// $Id$

#include "RenderSettings.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CliCommOutput.hh"
#include "InfoCommand.hh"

namespace openmsx {

RenderSettings::RenderSettings()
	: rendererInfo(*this),
	  msxConfig(MSXConfig::instance()),
	  infoCommand(InfoCommand::instance())
{
	Config* config = msxConfig.getConfigById("renderer");

	map<string, Accuracy> accMap;
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

	map<string, ScalerID> scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xsai"] = SCALER_SAI2X;
	scaler = new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap);

	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);

	infoCommand.registerTopic("renderer", &rendererInfo);
}

RenderSettings::~RenderSettings()
{
	infoCommand.unregisterTopic("renderer", &rendererInfo);

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

string RenderSettings::RendererInfo::execute(const vector<string> &tokens) const
	throw()
{
	string result;
	set<string> renderers;
	parent.getRenderer()->getPossibleValues(renderers);
	for (set<string>::const_iterator it = renderers.begin();
	     it != renderers.end(); ++it) {
		result += *it + '\n';
	}
	return result;
}

string RenderSettings::RendererInfo::help(const vector<string> &tokens) const
	throw()
{
	return "Shows a list of available renderers.\n";
}

} // namespace openmsx
