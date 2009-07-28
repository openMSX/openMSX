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
	accuracy.reset(new EnumSetting<Accuracy>(commandController,
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlace.reset(new BooleanSetting(commandController,
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkip.reset(new IntegerSetting(commandController,
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100));

	minFrameSkip.reset(new IntegerSetting(commandController,
		"minframeskip", "set the min amount of frameskip", 0, 0, 100));

	fullScreen.reset(new BooleanSetting(commandController,
		"fullscreen", "full screen display on/off", false));

	gamma.reset(new FloatSetting(commandController, "gamma",
		"amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	brightness.reset(new FloatSetting(commandController, "brightness",
		"brightness video setting: "
		"0 is normal, lower is darker, higher is brighter",
		0.0, -100.0, 100.0));

	contrast.reset(new FloatSetting(commandController, "contrast",
		"contrast video setting: "
		"0 is normal, lower is less contrast, higher is more contrast",
		0.0, -100.0, 100.0));

	colorMatrix.reset(new StringSetting(commandController, "color_matrix",
		"3x3 matrix to transform MSX RGB to host RGB, see manual for details",
		"{ 1 0 0 } { 0 1 0 } { 0 0 1 }"));
	colorMatrixChecker.reset(new ColorMatrixChecker(*this));
	colorMatrix->setChecker(colorMatrixChecker.get());

	glow.reset(new IntegerSetting(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	noise.reset(new FloatSetting(commandController,
		"noise", "amount of noise to add to the frame",
		0.0, 0.0, 100.0));

	horizontalBlur.reset(new IntegerSetting(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	videoSource.reset(new VideoSourceSetting(commandController));

	// Get user-preferred renderer from config.
	renderer = RendererFactory::createRendererSetting(commandController);

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
	scaleAlgorithm.reset(new EnumSetting<ScaleAlgorithm>(commandController,
		"scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, scalerMap));

	scaleFactor.reset(new IntegerSetting(commandController,
		"scale_factor", "scale factor",
		std::min(2, MAX_SCALE_FACTOR), MIN_SCALE_FACTOR, MAX_SCALE_FACTOR));

	scanlineAlpha.reset(new IntegerSetting(commandController,
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));

	limitSprites.reset(new BooleanSetting(commandController,
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true));

	EnumSetting<bool>::Map cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming.reset(new EnumSetting<bool>(commandController,
		"cmdtiming", "VDP command timing", false, cmdMap,
		Setting::DONT_SAVE));

	EnumSetting<DisplayDeform>::Map deformMap;
	deformMap["normal"] = DEFORM_NORMAL;
	deformMap["3d"] = DEFORM_3D;
	displayDeform.reset(new EnumSetting<DisplayDeform>(commandController,
		"display_deform", "Display deform (for the moment this only "
		"works with the SDLGL-PP renderer", DEFORM_NORMAL, deformMap));

	horizontalStretch.reset(new FloatSetting(commandController,
		"horizontal_stretch",
		"Amount of horizontal stretch: this many MSX pixels will be "
		"stretched over the complete width of the output screen.\n"
		"  320 = no stretch\n"
		"  256 = max stretch (no border visible anymore)\n"
		"  good values are 272 or 284\n"
		"This setting has only effect when using the SDLGL-PP renderer.",
		284.0, 256.0, 320.0));

	pointerHideDelay.reset(new FloatSetting(commandController,
		"pointer_hide_delay",
		"number of seconds after which the pointer is hidden in the openMSX "
		"window; negative = no hiding, 0 = immediately",
		1.0, -1.0, 60.0));
}

RenderSettings::~RenderSettings()
{
}

int RenderSettings::getBlurFactor() const
{
	return (horizontalBlur->getValue()) * 256 / 100;
}

int RenderSettings::getScanlineFactor() const
{
	return 255 - ((scanlineAlpha->getValue() * 255) / 100);
}

static double conv1(double x, double brightness, double contrast)
{
	return (x + brightness - 0.5) * contrast + 0.5;
}

static double conv2(double x, double gamma)
{
	return ::pow(std::min(std::max(0.0, x), 1.0), gamma);
}

void RenderSettings::transformRGB(double& r, double& g, double& b) const
{
	double brightness = getBrightness().getValue() / 100.0;
	double contrast = getContrast().getValue();
	contrast = (contrast >= 0.0) ? (1 + contrast / 25.0)
	                             : (1 + contrast / 125.0);
	r = conv1(r, brightness, contrast);
	g = conv1(g, brightness, contrast);
	b = conv1(b, brightness, contrast);

	double r2 = cm[0][0] * r + cm[0][1] * g + cm[0][2] * b;
	double g2 = cm[1][0] * r + cm[1][1] * g + cm[1][2] * b;
	double b2 = cm[2][0] * r + cm[2][1] * g + cm[2][2] * b;

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
	for (int i = 0; i < 3; ++i) {
		TclObject row = matrix.getListIndex(i);
		if (row.getListLength() != 3) {
			throw CommandException("each row must have 3 elements");
		}
		for (int j = 0; j < 3; ++j) {
			TclObject element = row.getListIndex(j);
			cm[i][j] = element.getDouble();
		}
	}
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
