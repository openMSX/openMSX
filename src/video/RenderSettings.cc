#include "RenderSettings.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Version.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include "components.hh"
#include <algorithm>
#include <iostream>
#include <cmath>

using namespace gl;

namespace openmsx {

EnumSetting<RenderSettings::ScaleAlgorithm>::Map RenderSettings::getScalerMap()
{
	EnumSetting<ScaleAlgorithm>::Map scalerMap = { { "simple", SCALER_SIMPLE } };
	if constexpr (MAX_SCALE_FACTOR > 1) {
		append(scalerMap, {{"SaI",        SCALER_SAI},
		                   {"ScaleNx",    SCALER_SCALE},
		                   {"hq",         SCALER_HQ},
		                   {"hqlite",     SCALER_HQLITE},
		                   {"RGBtriplet", SCALER_RGBTRIPLET},
		                   {"TV",         SCALER_TV}});
		if (!Version::RELEASE) {
			// This scaler is not ready yet for the upcoming 0.8.1
			// release, so disable it. As soon as it is ready we
			// can remove this test.
			scalerMap.emplace_back("MLAA", SCALER_MLAA);
		}
	}
	return scalerMap;
}

EnumSetting<RenderSettings::RendererID>::Map RenderSettings::getRendererMap()
{
	EnumSetting<RendererID>::Map rendererMap = {
		{ "none", DUMMY },// TODO: only register when in CliComm mode
		{ "SDL", SDL } };
#if COMPONENT_GL
	// compiled with OpenGL-2.0, still need to test whether
	// it's available at run time, but cannot be done here
	rendererMap.emplace_back("SDLGL-PP", SDLGL_PP);
#endif
	return rendererMap;
}

RenderSettings::RenderSettings(CommandController& commandController)
	: accuracySetting(commandController,
		"accuracy", "rendering accuracy", ACC_PIXEL,
		EnumSetting<Accuracy>::Map{
			{"screen", ACC_SCREEN},
			{"line",   ACC_LINE},
			{"pixel",  ACC_PIXEL}})

	, deinterlaceSetting(commandController,
		"deinterlace", "deinterlacing on/off", true)

	, deflickerSetting(commandController,
		"deflicker", "deflicker on/off", false)

	, maxFrameSkipSetting(commandController,
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100)

	, minFrameSkipSetting(commandController,
		"minframeskip", "set the min amount of frameskip", 0, 0, 100)

	, fullScreenSetting(commandController,
		"fullscreen", "full screen display on/off", false)

	, gammaSetting(commandController, "gamma",
		"amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0)

	, brightnessSetting(commandController, "brightness",
		"brightness video setting: "
		"0 is normal, lower is darker, higher is brighter",
		0.0, -100.0, 100.0)
	, contrastSetting(commandController, "contrast",
		"contrast video setting: "
		"0 is normal, lower is less contrast, higher is more contrast",
		0.0, -100.0, 100.0)

	, colorMatrixSetting(commandController,
		"color_matrix",
		"3x3 matrix to transform MSX RGB to host RGB, see manual for details",
		"{ 1 0 0 } { 0 1 0 } { 0 0 1 }")

	, glowSetting(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100)

	, noiseSetting(commandController,
		"noise", "amount of noise to add to the frame",
		0.0, 0.0, 100.0)

	, rendererSetting(commandController,
		"renderer", "rendering back-end used to display the MSX screen",
#if COMPONENT_GL
		SDLGL_PP,
#else
		SDL,
#endif
		getRendererMap())

	, horizontalBlurSetting(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100)

	, scaleAlgorithmSetting(
		commandController, "scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, getScalerMap())

	, scaleFactorSetting(commandController,
		"scale_factor", "scale factor",
		std::min(2, MAX_SCALE_FACTOR), MIN_SCALE_FACTOR, MAX_SCALE_FACTOR)

	, scanlineAlphaSetting(commandController,
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100)

	, limitSpritesSetting(commandController,
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true)

	, disableSpritesSetting(commandController,
		"disablesprites", "disable sprite rendering",
		false, Setting::DONT_SAVE)

	, cmdTimingSetting(commandController,
		"cmdtiming", "VDP command timing", false,
		EnumSetting<bool>::Map{{"real", false}, {"broken", true}},
		Setting::DONT_SAVE)

	, tooFastAccessSetting(commandController,
		"too_fast_vram_access",
		"Should too fast VDP VRAM access be correctly emulated.\n"
		"Possible values are:\n"
		" real -> too fast accesses are dropped\n"
		" ignore -> access speed is ignored, all accesses are executed",
		false,
		EnumSetting<bool>::Map{{"real", false }, {"ignore", true}},
		Setting::DONT_SAVE)

	, displayDeformSetting(
		commandController,
		"display_deform", "Display deform (for the moment this only "
		"works with the SDLGL-PP renderer)", DEFORM_NORMAL,
		EnumSetting<DisplayDeform>::Map{
			{"normal", DEFORM_NORMAL},
			{"3d",     DEFORM_3D}})

	, vSyncSetting(commandController,
		"vsync", "Synchronize page flip with the host screen vertical sync:\n"
		" on -> flip on host vsync: avoids tearing\n"
		" off -> immediate flip: might be more fluent when host framerate"
		" (typically 60Hz) differs from MSX framerate (50 or 60Hz)\n"
		"Currently this only affects the SDLGL-PP renderer.",
		true)

	// Many android devices are relatively low powered. Therefore use
	// no stretch (value 320) as default for Android because it gives
	// better performance
	, horizontalStretchSetting(commandController,
		"horizontal_stretch",
		"Amount of horizontal stretch: this many MSX pixels will be "
		"stretched over the complete width of the output screen.\n"
		"  320 = no stretch\n"
		"  256 = max stretch (no border visible anymore)\n"
		"  good values are 272 or 280\n"
		"This setting has only effect when using the SDLGL-PP renderer.",
		PLATFORM_ANDROID ? 320.0 : 280.0, 256.0, 320.0)

	, pointerHideDelaySetting(commandController,
		"pointer_hide_delay",
		"number of seconds after which the mouse pointer is hidden in the openMSX "
		"window; negative = no hiding, 0 = immediately",
		2.0, -1.0, 60.0)

	, interleaveBlackFrameSetting(commandController,
		"interleave_black_frame",
		"Insert a black frame in between each normal MSX frame. "
		"Useful on (100Hz+) lightboost enabled monitors to reduce "
		"motion blur and double frame artifacts.",
		false)
{
	brightnessSetting.attach(*this);
	contrastSetting  .attach(*this);
	updateBrightnessAndContrast();

	auto& interp = commandController.getInterpreter();
	colorMatrixSetting.setChecker([this, &interp](TclObject& newValue) {
		try {
			parseColorMatrix(interp, newValue);
		} catch (CommandException& e) {
			throw CommandException(
				"Invalid color matrix: ", e.getMessage());
		}
	});
	try {
		parseColorMatrix(interp, colorMatrixSetting.getValue());
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << '\n';
		cmIdentity = true;
	}

	// RendererSetting
	// Make sure the value 'none' never gets saved in settings.xml.
	// This happened in the following scenario:
	// - During startup, the renderer is forced to the value 'none'.
	// - If there's an error in the parsing of the command line (e.g.
	//   because an invalid option is passed) then openmsx will never
	//   get to the point where the actual renderer setting is restored
	// - After the error, the classes are destructed, part of that is
	//   saving the current settings. But without extra care, this would
	//   save renderer=none
	rendererSetting.setDontSaveValue(TclObject("none"));

	// A saved value 'none' can be very confusing. If so change it to default.
	if (rendererSetting.getEnum() == DUMMY) {
		rendererSetting.setValue(rendererSetting.getDefaultValue());
	}
	// set saved value as default
	rendererSetting.setRestoreValue(rendererSetting.getValue());

	rendererSetting.setEnum(DUMMY); // always start hidden
}

RenderSettings::~RenderSettings()
{
	brightnessSetting.detach(*this);
	contrastSetting  .detach(*this);
}

void RenderSettings::update(const Setting& setting) noexcept
{
	if (&setting == &brightnessSetting) {
		updateBrightnessAndContrast();
	} else if (&setting == &contrastSetting) {
		updateBrightnessAndContrast();
	} else {
		UNREACHABLE;
	}
}

void RenderSettings::updateBrightnessAndContrast()
{
	float contrastValue = getContrast();
	contrast = (contrastValue >= 0.0f) ? (1.0f + contrastValue /  25.0f)
	                                   : (1.0f + contrastValue / 125.0f);
	brightness = (getBrightness() / 100.0f - 0.5f) * contrast + 0.5f;
}

static float conv2(float x, float gamma)
{
	return ::powf(std::clamp(x, 0.0f, 1.0f), gamma);
}

float RenderSettings::transformComponent(float c) const
{
	float c2 = c * contrast + brightness;
	return conv2(c2, 1.0f / getGamma());
}

vec3 RenderSettings::transformRGB(vec3 rgb) const
{
	auto [r, g, b] = colorMatrix * (rgb * contrast + vec3(brightness));
	float gamma = 1.0f / getGamma();
	return {conv2(r, gamma),
	        conv2(g, gamma),
	        conv2(b, gamma)};
}

void RenderSettings::parseColorMatrix(Interpreter& interp, const TclObject& value)
{
	if (value.getListLength(interp) != 3) {
		throw CommandException("must have 3 rows");
	}
	bool identity = true;
	for (auto i : xrange(3)) {
		TclObject row = value.getListIndex(interp, i);
		if (row.getListLength(interp) != 3) {
			throw CommandException("each row must have 3 elements");
		}
		for (auto j : xrange(3)) {
			TclObject element = row.getListIndex(interp, j);
			float val = element.getDouble(interp);
			colorMatrix[j][i] = val;
			identity &= (val == (i == j ? 1.0f : 0.0f));
		}
	}
	cmIdentity = identity;
}

} // namespace openmsx
