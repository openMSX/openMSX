#ifndef RENDERSETTINGS_HH
#define RENDERSETTINGS_HH

#include "RendererFactory.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "FloatSetting.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "Observer.hh"
#include "noncopyable.hh"

namespace openmsx {

class CommandController;
class Interpreter;

/** Class containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings final : private Observer<Setting>, private noncopyable
{
public:
	/** Enumeration of Renderers known to openMSX.
	  * This is the full list, the list of available renderers may be smaller.
	  */
	enum RendererID { UNINITIALIZED, DUMMY, SDL,
	                  SDLGL_PP, SDLGL_FB16, SDLGL_FB32 };
	typedef EnumSetting<RendererID> RendererSetting;

	/** Render accuracy: granularity of the rendered area.
	  */
	enum Accuracy { ACC_SCREEN, ACC_LINE, ACC_PIXEL };

	/** Scaler algorithm
	  */
	enum ScaleAlgorithm {
		SCALER_SIMPLE, SCALER_SAI, SCALER_SCALE,
		SCALER_HQ, SCALER_HQLITE, SCALER_RGBTRIPLET, SCALER_TV, SCALER_MLAA,
		NO_SCALER
	};

	enum DisplayDeform {
		DEFORM_NORMAL, DEFORM_3D
	};

	explicit RenderSettings(CommandController& commandController);
	~RenderSettings();

	/** Accuracy [screen, line, pixel]. */
	EnumSetting<Accuracy>& getAccuracy() { return accuracySetting; }

	/** Deinterlacing [on, off]. */
	BooleanSetting& getDeinterlace() { return deinterlaceSetting; }

	/** Deflicker [on, off]. */
	BooleanSetting& getDeflicker() { return deflickerSetting; }

	/** The current max frameskip. */
	IntegerSetting& getMaxFrameSkip() { return maxFrameSkipSetting; }

	/** The current min frameskip. */
	IntegerSetting& getMinFrameSkip() { return minFrameSkipSetting; }

	/** Full screen [on, off]. */
	BooleanSetting& getFullScreen() { return fullScreenSetting; }

	/** The amount of gamma correction. */
	FloatSetting& getGamma() { return gammaSetting; }

	/** Brightness video setting. */
	FloatSetting& getBrightness() { return brightnessSetting; }

	/** Contrast video setting. */
	FloatSetting& getContrast() { return contrastSetting; }

	/** Color matrix setting. */
	StringSetting& getColorMatrix() { return colorMatrixSetting; }

	/** Returns true iff the current color matrix is the identity matrix. */
	bool isColorMatrixIdentity() const { return cmIdentity; }

	/** The amount of glow [0..100]. */
	IntegerSetting& getGlow() { return glowSetting; }

	/** The amount of noise to add to the frame. */
	FloatSetting& getNoise() { return noiseSetting; }

	/** The amount of horizontal blur [0..256]. */
	int getBlurFactor() const;

	/** The alpha value [0..255] of the gap between scanlines. */
	int getScanlineFactor() const;

	/** The amount of space [0..1] between scanlines. */
	float getScanlineGap() const;

	/** The current renderer. */
	RendererSetting& getRenderer() { return rendererSetting; }

	/** The current scaling algorithm. */
	EnumSetting<ScaleAlgorithm>& getScaleAlgorithm() {
		return scaleAlgorithmSetting;
	}

	/** The current scaling factor. */
	IntegerSetting& getScaleFactor() { return scaleFactorSetting; }

	/** Limit number of sprites per line?
	  * If true, limit number of sprites per line as real VDP does.
	  * If false, display all sprites.
	  * For accurate emulation, this setting should be on.
	  * Turning it off can improve games with a lot of flashing sprites,
	  * such as Aleste. */
	BooleanSetting& getLimitSprites() { return limitSpritesSetting; }

	/** Disable sprite rendering? */
	BooleanSetting& getDisableSprites() { return disableSpritesSetting; }

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users. */
	EnumSetting<bool>& getCmdTiming() { return cmdTimingSetting; }

	/** TooFastAccess [real, ignored].
	  * Indicates whether too fast VDP VRAM access should be correctly
	  * emulated (= some accesses are dropped) or ignored (= all accesses
	  * are correctly executed). */
	EnumSetting<bool>& getTooFastAccess() { return tooFastAccessSetting; }

	/** Display deformation (normal, 3d)
	  * ATM this only works when using the SDLGL-PP renderer. */
	EnumSetting<DisplayDeform>& getDisplayDeform() {
		return displayDeformSetting;
	}

	/** Amount of horizontal stretch.
	  * This number represents the amount of MSX pixels (normal width) that
	  * will be stretched to the complete width of the host window.
	  * ATM this setting only has effect when using the SDLGL-PP renderer. */
	FloatSetting& getHorizontalStretch() {
		return horizontalStretchSetting;
	}

	/** The amount of time until the pointer is hidden in the openMSX
	  * window. negative means: no hiding, 0 means immediately. */
	FloatSetting& getPointerHideDelay() {
		return pointerHideDelaySetting;
	}

	/** Is black frame interleaving enabled? */
	BooleanSetting& getInterleaveBlackFrame() {
		return interleaveBlackFrameSetting;
	}

	/** Apply brightness, contrast and gamma transformation on the input
	  * color component. The component is expected to be in the range
	  * [0.0 .. 1.0] but it's not an error if it lays outside of this range.
	  * The return value is guaranteed to lay inside this range.
	  * This method skips the cross-influence of color components on each
	  * other that is controlled by the "color_matrix" setting.
	  */
	double transformComponent(double c) const;

	/** Apply brightness, contrast and gamma transformation on the input
	  * color. The R, G and B component are expected to be in the range
	  * [0.0 .. 1.0] but it's not an error if a component lays outside of
	  * this range. After transformation it's guaranteed all components
	  * lay inside this range.
	  */
	void transformRGB(double& r, double& g, double& b) const;

private:
	static EnumSetting<ScaleAlgorithm>::Map getScalerMap();
	static EnumSetting<RendererID>::Map getRendererMap();

	// Observer:
	void update(const Setting&) override;

	/** Sets the "brightness" and "contrast" fields according to the setting
	  * values.
	  */
	void updateBrightnessAndContrast();

	void parseColorMatrix(Interpreter& interp, const TclObject& value);

	EnumSetting<Accuracy> accuracySetting;
	BooleanSetting deinterlaceSetting;
	BooleanSetting deflickerSetting;
	IntegerSetting maxFrameSkipSetting;
	IntegerSetting minFrameSkipSetting;
	BooleanSetting fullScreenSetting;
	FloatSetting gammaSetting;
	FloatSetting brightnessSetting;
	FloatSetting contrastSetting;
	StringSetting colorMatrixSetting;
	IntegerSetting glowSetting;
	FloatSetting noiseSetting;
	RendererSetting rendererSetting;
	IntegerSetting horizontalBlurSetting;
	EnumSetting<ScaleAlgorithm> scaleAlgorithmSetting;
	IntegerSetting scaleFactorSetting;
	IntegerSetting scanlineAlphaSetting;
	BooleanSetting limitSpritesSetting;
	BooleanSetting disableSpritesSetting;
	EnumSetting<bool> cmdTimingSetting;
	EnumSetting<bool> tooFastAccessSetting;
	EnumSetting<DisplayDeform> displayDeformSetting;
	FloatSetting horizontalStretchSetting;
	FloatSetting pointerHideDelaySetting;
	BooleanSetting interleaveBlackFrameSetting;

	double brightness;
	double contrast;

	/** Parsed color matrix, kept in sync with colorMatrix setting. */
	double cm[3][3];
	/** True iff color matrix is identity matrix. */
	bool cmIdentity;
};

} // namespace openmsx

#endif
