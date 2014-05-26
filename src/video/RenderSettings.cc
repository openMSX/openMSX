#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "EnumSetting.hh"
#include "CommandException.hh"
#include "Version.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "build-info.hh"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace openmsx {

RenderSettings::RenderSettings(CommandController& commandController)
{
	EnumSetting<Accuracy>::Map accMap = {
		{ "screen", ACC_SCREEN },
		{ "line",   ACC_LINE },
		{ "pixel",  ACC_PIXEL } };
	accuracySetting = make_unique<EnumSetting<Accuracy>>(commandController,
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap);

	deinterlaceSetting = make_unique<BooleanSetting>(commandController,
		"deinterlace", "deinterlacing on/off", true);

	deflickerSetting = make_unique<BooleanSetting>(commandController,
		"deflicker", "deflicker on/off", false);

	maxFrameSkipSetting = make_unique<IntegerSetting>(commandController,
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100);

	minFrameSkipSetting = make_unique<IntegerSetting>(commandController,
		"minframeskip", "set the min amount of frameskip", 0, 0, 100);

	fullScreenSetting = make_unique<BooleanSetting>(commandController,
		"fullscreen", "full screen display on/off", false);

	gammaSetting = make_unique<FloatSetting>(commandController, "gamma",
		"amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0);

	brightnessSetting = make_unique<FloatSetting>(commandController, "brightness",
		"brightness video setting: "
		"0 is normal, lower is darker, higher is brighter",
		0.0, -100.0, 100.0);
	contrastSetting = make_unique<FloatSetting>(commandController, "contrast",
		"contrast video setting: "
		"0 is normal, lower is less contrast, higher is more contrast",
		0.0, -100.0, 100.0);
	brightnessSetting->attach(*this);
	contrastSetting->attach(*this);
	updateBrightnessAndContrast();

	colorMatrixSetting = make_unique<StringSetting>(commandController,
		"color_matrix",
		"3x3 matrix to transform MSX RGB to host RGB, see manual for details",
		"{ 1 0 0 } { 0 1 0 } { 0 0 1 }");
	colorMatrixSetting->setChecker([this](TclObject& newValue) {
		try {
			parseColorMatrix(newValue);
		} catch (CommandException& e) {
			throw CommandException(
				"Invalid color matrix: " + e.getMessage());
		}
	});
	try {
		parseColorMatrix(colorMatrixSetting->getValue());
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << std::endl;
		cmIdentity = true;
	}

	glowSetting = make_unique<IntegerSetting>(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100);

	noiseSetting = make_unique<FloatSetting>(commandController,
		"noise", "amount of noise to add to the frame",
		0.0, 0.0, 100.0);

	horizontalBlurSetting = make_unique<IntegerSetting>(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100);

	// Get user-preferred renderer from config.
	rendererSetting = RendererFactory::createRendererSetting(commandController);

	EnumSetting<ScaleAlgorithm>::Map scalerMap = { { "simple", SCALER_SIMPLE } };
	if (MAX_SCALE_FACTOR > 1) {
		scalerMap.insert(end(scalerMap), {
			{ "SaI",        SCALER_SAI },
			{ "ScaleNx",    SCALER_SCALE },
			{ "hq",         SCALER_HQ },
			{ "hqlite",     SCALER_HQLITE },
			{ "RGBtriplet", SCALER_RGBTRIPLET },
			{ "TV",         SCALER_TV } });
		if (!Version::RELEASE) {
			// This scaler is not ready yet for the upcoming 0.8.1
			// release, so disable it. As soon as it is ready we
			// can remove this test.
			scalerMap.emplace_back("MLAA", SCALER_MLAA);
		}
	}
	scaleAlgorithmSetting = make_unique<EnumSetting<ScaleAlgorithm>>(
		commandController, "scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, scalerMap);

	scaleFactorSetting = make_unique<IntegerSetting>(commandController,
		"scale_factor", "scale factor",
		std::min(2, MAX_SCALE_FACTOR), MIN_SCALE_FACTOR, MAX_SCALE_FACTOR);

	scanlineAlphaSetting = make_unique<IntegerSetting>(commandController,
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);

	limitSpritesSetting = make_unique<BooleanSetting>(commandController,
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true);

	disableSpritesSetting = make_unique<BooleanSetting>(commandController,
		"disablesprites", "disable sprite rendering",
		false, Setting::DONT_SAVE);

	EnumSetting<bool>::Map cmdMap = {
		{ "real",   false },
		{ "broken", true } };
	cmdTimingSetting = make_unique<EnumSetting<bool>>(commandController,
		"cmdtiming", "VDP command timing", false, cmdMap,
		Setting::DONT_SAVE);

	EnumSetting<bool>::Map accessMap = {
		{ "real",   false },
		{ "ignore", true } };
	tooFastAccessSetting = make_unique<EnumSetting<bool>>(commandController,
		"too_fast_vram_access",
		"Should too fast VDP VRAM access be correctly emulated.\n"
		"Possible values are:\n"
		" real -> too fast accesses are dropped\n"
		" ignore -> access speed is ignored, all accesses are executed",
		false, accessMap, Setting::DONT_SAVE);

	EnumSetting<DisplayDeform>::Map deformMap = {
		{ "normal", DEFORM_NORMAL },
		{ "3d",     DEFORM_3D } };
	displayDeformSetting = make_unique<EnumSetting<DisplayDeform>>(
		commandController,
		"display_deform", "Display deform (for the moment this only "
		"works with the SDLGL-PP renderer", DEFORM_NORMAL, deformMap);

	// Many android devices are relatively low powered. Therefore use
	// no stretch (value 320) as default for Android because it gives
	// better performance
	horizontalStretchSetting = make_unique<FloatSetting>(commandController,
		"horizontal_stretch",
		"Amount of horizontal stretch: this many MSX pixels will be "
		"stretched over the complete width of the output screen.\n"
		"  320 = no stretch\n"
		"  256 = max stretch (no border visible anymore)\n"
		"  good values are 272 or 280\n"
		"This setting has only effect when using the SDLGL-PP renderer.",
		PLATFORM_ANDROID ? 320.0 : 280.0, 256.0, 320.0);

	pointerHideDelaySetting = make_unique<FloatSetting>(commandController,
		"pointer_hide_delay",
		"number of seconds after which the mouse pointer is hidden in the openMSX "
		"window; negative = no hiding, 0 = immediately",
		2.0, -1.0, 60.0);

	interleaveBlackFrameSetting = make_unique<BooleanSetting>(commandController,
		"interleave_black_frame",
		"Insert a black frame in between each normal MSX frame. "
		"Useful on (100Hz+) lightboost enabled monitors to reduce "
		"motion blur and double frame artifacts.",
		false);
}

RenderSettings::~RenderSettings()
{
	brightnessSetting->detach(*this);
	contrastSetting->detach(*this);
}

int RenderSettings::getBlurFactor() const
{
	return (horizontalBlurSetting->getInt()) * 256 / 100;
}

int RenderSettings::getScanlineFactor() const
{
	return 255 - ((scanlineAlphaSetting->getInt() * 255) / 100);
}

float RenderSettings::getScanlineGap() const
{
	return scanlineAlphaSetting->getInt() * 0.01f;
}

void RenderSettings::update(const Setting& setting)
{
	if (&setting == brightnessSetting.get()) {
		updateBrightnessAndContrast();
	} else if (&setting == contrastSetting.get()) {
		updateBrightnessAndContrast();
	} else {
		UNREACHABLE;
	}
}

void RenderSettings::updateBrightnessAndContrast()
{
	double contrastValue = getContrast().getDouble();
	contrast = (contrastValue >= 0.0) ? (1.0 + contrastValue / 25.0)
	                                  : (1.0 + contrastValue / 125.0);
	double brightnessValue = getBrightness().getDouble();
	brightness = (brightnessValue / 100.0 - 0.5) * contrast + 0.5;
}

static double conv2(double x, double gamma)
{
	return ::pow(std::min(std::max(0.0, x), 1.0), gamma);
}

double RenderSettings::transformComponent(double c) const
{
	double c2 = c * contrast + brightness;
	double gamma = 1.0 / getGamma().getDouble();
	return conv2(c2, gamma);
}

void RenderSettings::transformRGB(double& r, double& g, double& b) const
{
	double rbc = r * contrast + brightness;
	double gbc = g * contrast + brightness;
	double bbc = b * contrast + brightness;

	double r2 = cm[0][0] * rbc + cm[0][1] * gbc + cm[0][2] * bbc;
	double g2 = cm[1][0] * rbc + cm[1][1] * gbc + cm[1][2] * bbc;
	double b2 = cm[2][0] * rbc + cm[2][1] * gbc + cm[2][2] * bbc;

	double gamma = 1.0 / getGamma().getDouble();
	r = conv2(r2, gamma);
	g = conv2(g2, gamma);
	b = conv2(b2, gamma);
}

void RenderSettings::parseColorMatrix(const TclObject& value)
{
	if (value.getListLength() != 3) {
		throw CommandException("must have 3 rows");
	}
	bool identity = true;
	for (int i = 0; i < 3; ++i) {
		TclObject row = value.getListIndex(i);
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

} // namespace openmsx
