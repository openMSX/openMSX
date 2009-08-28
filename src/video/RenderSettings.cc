// $Id$

#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "VideoSourceSetting.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "build-info.hh"
#include <algorithm>
#include <cmath>

namespace openmsx {

class ColorMatrixChecker : public SettingChecker<StringSettingPolicy>
{
public:
	explicit ColorMatrixChecker(RenderSettings& renderSettings);
	virtual void check(SettingImpl<StringSettingPolicy>& setting,
	                   std::string& newValue);
private:
	RenderSettings& renderSettings;
};


RenderSettings::RenderSettings(CommandController& commandController_)
	: commandController(commandController_)
{
	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracySetting.reset(new EnumSetting<Accuracy>(commandController,
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlaceSetting.reset(new BooleanSetting(commandController,
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkipSetting.reset(new IntegerSetting(commandController,
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100));

	minFrameSkipSetting.reset(new IntegerSetting(commandController,
		"minframeskip", "set the min amount of frameskip", 0, 0, 100));

	fullScreenSetting.reset(new BooleanSetting(commandController,
		"fullscreen", "full screen display on/off", false));

	gammaSetting.reset(new FloatSetting(commandController, "gamma",
		"amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	brightnessSetting.reset(new FloatSetting(commandController, "brightness",
		"brightness video setting: "
		"0 is normal, lower is darker, higher is brighter",
		0.0, -100.0, 100.0));
	contrastSetting.reset(new FloatSetting(commandController, "contrast",
		"contrast video setting: "
		"0 is normal, lower is less contrast, higher is more contrast",
		0.0, -100.0, 100.0));
	brightnessSetting->attach(*this);
	contrastSetting->attach(*this);
	updateBrightnessAndContrast();

	colorMatrixSetting.reset(new StringSetting(commandController,
		"color_matrix",
		"3x3 matrix to transform MSX RGB to host RGB, see manual for details",
		"{ 1 0 0 } { 0 1 0 } { 0 0 1 }"));
	colorMatrixChecker.reset(new ColorMatrixChecker(*this));
	colorMatrixSetting->setChecker(colorMatrixChecker.get());

	glowSetting.reset(new IntegerSetting(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	noiseSetting.reset(new FloatSetting(commandController,
		"noise", "amount of noise to add to the frame",
		0.0, 0.0, 100.0));

	horizontalBlurSetting.reset(new IntegerSetting(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	videoSourceSetting.reset(new VideoSourceSetting(commandController));

	// Get user-preferred renderer from config.
	rendererSetting = RendererFactory::createRendererSetting(commandController);

	EnumSetting<ScaleAlgorithm>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	if (MAX_SCALE_FACTOR > 1) {
		scalerMap["SaI"] = SCALER_SAI;
		scalerMap["ScaleNx"] = SCALER_SCALE;
		scalerMap["hq"] = SCALER_HQ;
		scalerMap["hqlite"] = SCALER_HQLITE;
		scalerMap["RGBtriplet"] = SCALER_RGBTRIPLET;
		scalerMap["TV"] = SCALER_TV;
	}
	scaleAlgorithmSetting.reset(new EnumSetting<ScaleAlgorithm>(commandController,
		"scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, scalerMap));

	scaleFactorSetting.reset(new IntegerSetting(commandController,
		"scale_factor", "scale factor",
		std::min(2, MAX_SCALE_FACTOR), MIN_SCALE_FACTOR, MAX_SCALE_FACTOR));

	scanlineAlphaSetting.reset(new IntegerSetting(commandController,
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));

	limitSpritesSetting.reset(new BooleanSetting(commandController,
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true));

	EnumSetting<bool>::Map cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTimingSetting.reset(new EnumSetting<bool>(commandController,
		"cmdtiming", "VDP command timing", false, cmdMap,
		Setting::DONT_SAVE));

	EnumSetting<DisplayDeform>::Map deformMap;
	deformMap["normal"] = DEFORM_NORMAL;
	deformMap["3d"] = DEFORM_3D;
	displayDeformSetting.reset(new EnumSetting<DisplayDeform>(commandController,
		"display_deform", "Display deform (for the moment this only "
		"works with the SDLGL-PP renderer", DEFORM_NORMAL, deformMap));

	horizontalStretchSetting.reset(new FloatSetting(commandController,
		"horizontal_stretch",
		"Amount of horizontal stretch: this many MSX pixels will be "
		"stretched over the complete width of the output screen.\n"
		"  320 = no stretch\n"
		"  256 = max stretch (no border visible anymore)\n"
		"  good values are 272 or 284\n"
		"This setting has only effect when using the SDLGL-PP renderer.",
		284.0, 256.0, 320.0));

	pointerHideDelaySetting.reset(new FloatSetting(commandController,
		"pointer_hide_delay",
		"number of seconds after which the pointer is hidden in the openMSX "
		"window; negative = no hiding, 0 = immediately",
		1.0, -1.0, 60.0));
}

RenderSettings::~RenderSettings()
{
	brightnessSetting->detach(*this);
	contrastSetting->detach(*this);
}

int RenderSettings::getBlurFactor() const
{
	return (horizontalBlurSetting->getValue()) * 256 / 100;
}

int RenderSettings::getScanlineFactor() const
{
	return 255 - ((scanlineAlphaSetting->getValue() * 255) / 100);
}

void RenderSettings::update(const Setting& setting)
{
	if (&setting == brightnessSetting.get()) {
		updateBrightnessAndContrast();
	} else if (&setting == contrastSetting.get()) {
		updateBrightnessAndContrast();
	} else {
		assert(false);
	}
}

void RenderSettings::updateBrightnessAndContrast()
{
	double contrastValue = getContrast().getValue();
	contrast = (contrastValue >= 0.0) ? (1.0 + contrastValue / 25.0)
	                                  : (1.0 + contrastValue / 125.0);
	double brightnessValue = getBrightness().getValue();
	brightness = (brightnessValue / 100.0 - 0.5) * contrast + 0.5;
}

static double conv2(double x, double gamma)
{
	return ::pow(std::min(std::max(0.0, x), 1.0), gamma);
}

void RenderSettings::transformRGB(double& r, double& g, double& b) const
{
	r = r * contrast + brightness;
	g = g * contrast + brightness;
	b = b * contrast + brightness;

	double r2, g2, b2;
	if (cmIdentity) {
		// Most users use the "normal" monitor type; making this a special case
		// speeds up palette precalculation a lot.
		r2 = r;
		g2 = g;
		b2 = b;
	} else {
		r2 = cm[0][0] * r + cm[0][1] * g + cm[0][2] * b;
		g2 = cm[1][0] * r + cm[1][1] * g + cm[1][2] * b;
		b2 = cm[2][0] * r + cm[2][1] * g + cm[2][2] * b;
	}

	double gamma = 1.0 / getGamma().getValue();
	r = conv2(r2, gamma);
	g = conv2(g2, gamma);
	b = conv2(b2, gamma);
}

void RenderSettings::parseColorMatrix(const std::string& value)
{
	TclObject matrix(commandController.getInterpreter());
	matrix.setString(value);
	if (matrix.getListLength() != 3) {
		throw CommandException("must have 3 rows");
	}
	bool identity = true;
	for (int i = 0; i < 3; ++i) {
		TclObject row = matrix.getListIndex(i);
		if (row.getListLength() != 3) {
			throw CommandException("each row must have 3 elements");
		}
		for (int j = 0; j < 3; ++j) {
			TclObject element = row.getListIndex(j);
			double value = element.getDouble();
			cm[i][j] = value;
			identity &= (value == (i == j ? 1.0 : 0.0));
		}
	}
	cmIdentity = identity;
}


ColorMatrixChecker::ColorMatrixChecker(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
}

void ColorMatrixChecker::check(SettingImpl<StringSettingPolicy>& /*setting*/,
                               std::string& newValue)
{
	try {
		renderSettings.parseColorMatrix(newValue);
	} catch (CommandException& e) {
		throw CommandException("Invalid color matrix: " + e.getMessage());
	}
}

} // namespace openmsx
