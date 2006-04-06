// $Id$

#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "VideoSourceSetting.hh"
#include <cmath>

namespace openmsx {

class ColorMatrixChecker : public SettingChecker<StringSettingPolicy>
{
public:
	ColorMatrixChecker(RenderSettings& renderSettings);
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
	scalerMap["SaI"] = SCALER_SAI;
	scalerMap["ScaleNx"] = SCALER_SCALE;
	scalerMap["hq"] = SCALER_HQ;
	scalerMap["hqlite"] = SCALER_HQLITE;
	scalerMap["RGBtriplet"] = SCALER_RGBTRIPLET;
	scalerMap["TV"] = SCALER_TV;
	scaleAlgorithm.reset(new EnumSetting<ScaleAlgorithm>(commandController,
		"scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, scalerMap));

	scaleFactor.reset(new IntegerSetting(commandController,
		"scale_factor", "scale factor", 2, 1, 4));

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

void RenderSettings::transformRGB(double& r, double& g, double& b)
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
