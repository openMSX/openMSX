#ifndef RENDERSETTINGS_HH
#define RENDERSETTINGS_HH

#include "RendererFactory.hh"
#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class Setting;
class IntegerSetting;
class FloatSetting;
class BooleanSetting;
class StringSetting;
class TclObject;

/** Class containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings : private Observer<Setting>, private noncopyable
{
public:
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
	EnumSetting<Accuracy>& getAccuracy() const { return *accuracySetting; }

	/** Deinterlacing [on, off]. */
	BooleanSetting& getDeinterlace() const { return *deinterlaceSetting; }

	/** The current max frameskip. */
	IntegerSetting& getMaxFrameSkip() const { return *maxFrameSkipSetting; }

	/** The current min frameskip. */
	IntegerSetting& getMinFrameSkip() const { return *minFrameSkipSetting; }

	/** Full screen [on, off]. */
	BooleanSetting& getFullScreen() const { return *fullScreenSetting; }

	/** The amount of gamma correction. */
	FloatSetting& getGamma() const { return *gammaSetting; }

	/** Brightness video setting. */
	FloatSetting& getBrightness() const { return *brightnessSetting; }

	/** Contrast video setting. */
	FloatSetting& getContrast() const { return *contrastSetting; }

	/** Color matrix setting. */
	StringSetting& getColorMatrix() const { return *colorMatrixSetting; }

	/** Returns true iff the current color matrix is the identity matrix. */
	bool isColorMatrixIdentity() const { return cmIdentity; }

	/** The amount of glow [0..100]. */
	IntegerSetting& getGlow() const { return *glowSetting; }

	/** The amount of noise to add to the frame. */
	FloatSetting& getNoise() const { return *noiseSetting; }

	/** The amount of horizontal blur [0..256]. */
	int getBlurFactor() const;

	/** The alpha value [0..255] of the gap between scanlines. */
	int getScanlineFactor() const;

	/** The amount of space [0..1] between scanlines. */
	float getScanlineGap() const;

	/** The current renderer. */
	RendererFactory::RendererSetting& getRenderer() const {
		return *rendererSetting;
	}

	/** The current scaling algorithm. */
	EnumSetting<ScaleAlgorithm>& getScaleAlgorithm() const {
		return *scaleAlgorithmSetting;
	}

	/** The current scaling factor. */
	IntegerSetting& getScaleFactor() const { return *scaleFactorSetting; }

	/** Limit number of sprites per line?
	  * If true, limit number of sprites per line as real VDP does.
	  * If false, display all sprites.
	  * For accurate emulation, this setting should be on.
	  * Turning it off can improve games with a lot of flashing sprites,
	  * such as Aleste. */
	BooleanSetting& getLimitSprites() const { return *limitSpritesSetting; }

	/** Disable sprite rendering? */
	BooleanSetting& getDisableSprites() const { return *disableSpritesSetting; }

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users. */
	EnumSetting<bool>& getCmdTiming() const { return *cmdTimingSetting; }

	/** TooFastAccess [real, ignored].
	  * Indicates whether too fast VDP VRAM access should be correctly
	  * emulated (= some accesses are dropped) or ignored (= all accesses
	  * are correctly executed). */
	EnumSetting<bool>& getTooFastAccess() const { return *tooFastAccessSetting; }

	/** Display deformation (normal, 3d)
	  * ATM this only works when using the SDLGL-PP renderer. */
	EnumSetting<DisplayDeform>& getDisplayDeform() const {
		return *displayDeformSetting;
	}

	/** Amount of horizontal stretch.
	  * This number represents the amount of MSX pixels (normal width) that
	  * will be stretched to the complete width of the host window.
	  * ATM this setting only has effect when using the SDLGL-PP renderer. */
	FloatSetting& getHorizontalStretch() const {
		return *horizontalStretchSetting;
	}

	/** The amount of time until the pointer is hidden in the openMSX
	  * window. negative means: no hiding, 0 means immediately. */
	FloatSetting& getPointerHideDelay() const {
		return *pointerHideDelaySetting;
	}

	/** Is black frame interleaving enabled? */
	BooleanSetting& getInterleaveBlackFrame() const {
		return *interleaveBlackFrameSetting;
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
	// Observer:
	void update(const Setting&);

	/** Sets the "brightness" and "contrast" fields according to the setting
	  * values.
	  */
	void updateBrightnessAndContrast();

	void parseColorMatrix(const TclObject& value);

	std::unique_ptr<EnumSetting<Accuracy>> accuracySetting;
	std::unique_ptr<EnumSetting<bool>> cmdTimingSetting;
	std::unique_ptr<EnumSetting<bool>> tooFastAccessSetting;
	std::unique_ptr<BooleanSetting> deinterlaceSetting;
	std::unique_ptr<BooleanSetting> fullScreenSetting;
	std::unique_ptr<FloatSetting> gammaSetting;
	std::unique_ptr<FloatSetting> brightnessSetting;
	std::unique_ptr<FloatSetting> contrastSetting;
	std::unique_ptr<StringSetting> colorMatrixSetting;
	std::unique_ptr<IntegerSetting> glowSetting;
	std::unique_ptr<FloatSetting> noiseSetting;
	std::unique_ptr<IntegerSetting> horizontalBlurSetting;
	std::unique_ptr<BooleanSetting> limitSpritesSetting;
	std::unique_ptr<BooleanSetting> disableSpritesSetting;
	std::unique_ptr<IntegerSetting> maxFrameSkipSetting;
	std::unique_ptr<IntegerSetting> minFrameSkipSetting;
	std::unique_ptr<RendererFactory::RendererSetting> rendererSetting;
	std::unique_ptr<EnumSetting<ScaleAlgorithm>> scaleAlgorithmSetting;
	std::unique_ptr<IntegerSetting> scaleFactorSetting;
	std::unique_ptr<IntegerSetting> scanlineAlphaSetting;
	std::unique_ptr<EnumSetting<DisplayDeform>> displayDeformSetting;
	std::unique_ptr<FloatSetting> horizontalStretchSetting;
	std::unique_ptr<FloatSetting> pointerHideDelaySetting;
	std::unique_ptr<BooleanSetting> interleaveBlackFrameSetting;

	double brightness;
	double contrast;

	/** Parsed color matrix, kept in sync with colorMatrix setting. */
	double cm[3][3];
	/** True iff color matrix is identity matrix. */
	bool cmIdentity;
};

} // namespace openmsx

#endif
