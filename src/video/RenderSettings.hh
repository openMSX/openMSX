#ifndef RENDERSETTINGS_HH
#define RENDERSETTINGS_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "FloatSetting.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "Observer.hh"
#include "gl_mat.hh"
#include "narrow.hh"

namespace openmsx {

class CommandController;
class Interpreter;

/** Class containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings final : private Observer<Setting>
{
public:
	/** Enumeration of Renderers known to openMSX.
	  * This is the full list, the list of available renderers may be smaller.
	  */
	enum RendererID { UNINITIALIZED, DUMMY, SDL, SDLGL_PP };
	using RendererSetting = EnumSetting<RendererID>;

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
	[[nodiscard]] Accuracy getAccuracy() const { return accuracySetting.getEnum(); }

	/** Deinterlacing [on, off]. */
	[[nodiscard]] bool getDeinterlace() const { return deinterlaceSetting.getBoolean(); }

	/** Deflicker [on, off]. */
	[[nodiscard]] bool getDeflicker() const { return deflickerSetting.getBoolean(); }

	/** The current max frameskip. */
	[[nodiscard]] IntegerSetting& getMaxFrameSkipSetting() { return maxFrameSkipSetting; }
	[[nodiscard]] int getMaxFrameSkip() const { return maxFrameSkipSetting.getInt(); }

	/** The current min frameskip. */
	[[nodiscard]] IntegerSetting& getMinFrameSkipSetting() { return minFrameSkipSetting; }
	[[nodiscard]] int getMinFrameSkip() const { return minFrameSkipSetting.getInt(); }

	/** Full screen [on, off]. */
	[[nodiscard]] BooleanSetting& getFullScreenSetting() { return fullScreenSetting; }
	[[nodiscard]] bool getFullScreen() const { return fullScreenSetting.getBoolean(); }

	/** The amount of gamma correction. */
	[[nodiscard]] FloatSetting& getGammaSetting() { return gammaSetting; }
	[[nodiscard]] float getGamma() const { return gammaSetting.getFloat(); }

	/** Brightness video setting. */
	[[nodiscard]] FloatSetting& getBrightnessSetting() { return brightnessSetting; }
	[[nodiscard]] float getBrightness() const { return brightnessSetting.getFloat(); }

	/** Contrast video setting. */
	[[nodiscard]] FloatSetting& getContrastSetting() { return contrastSetting; }
	[[nodiscard]] float getContrast() const { return contrastSetting.getFloat(); }

	/** Color matrix setting. */
	[[nodiscard]] StringSetting& getColorMatrixSetting() { return colorMatrixSetting; }
	/** Returns true iff the current color matrix is the identity matrix. */
	[[nodiscard]] bool isColorMatrixIdentity() const { return cmIdentity; }

	/** The amount of glow [0..100]. */
	[[nodiscard]] int getGlow() const { return glowSetting.getInt(); }

	/** The amount of noise to add to the frame. */
	[[nodiscard]] FloatSetting& getNoiseSetting() { return noiseSetting; }
	[[nodiscard]] float getNoise() const { return noiseSetting.getFloat(); }

	/** The amount of horizontal blur [0..256]. */
	[[nodiscard]] int getBlurFactor() const {
		return (horizontalBlurSetting.getInt()) * 256 / 100;
	}

	/** The alpha value [0..255] of the gap between scanlines. */
	[[nodiscard]] int getScanlineFactor() const {
		return 255 - ((scanlineAlphaSetting.getInt() * 255) / 100);
	}

	/** The amount of space [0..1] between scanlines. */
	[[nodiscard]] float getScanlineGap() const {
		return narrow<float>(scanlineAlphaSetting.getInt()) * 0.01f;
	}

	/** The current renderer. */
	[[nodiscard]] RendererSetting& getRendererSetting() { return rendererSetting; }
	[[nodiscard]] RendererID getRenderer() const { return rendererSetting.getEnum(); }

	/** The current scaling algorithm. */
	[[nodiscard]] ScaleAlgorithm getScaleAlgorithm() const {
		return scaleAlgorithmSetting.getEnum();
	}

	/** The current scaling factor. */
	[[nodiscard]] IntegerSetting& getScaleFactorSetting() { return scaleFactorSetting; }
	[[nodiscard]] int getScaleFactor() const { return scaleFactorSetting.getInt(); }

	/** Limit number of sprites per line?
	  * If true, limit number of sprites per line as real VDP does.
	  * If false, display all sprites.
	  * For accurate emulation, this setting should be on.
	  * Turning it off can improve games with a lot of flashing sprites,
	  * such as Aleste. */
	[[nodiscard]] BooleanSetting& getLimitSpritesSetting() { return limitSpritesSetting; }

	/** Disable sprite rendering? */
	[[nodiscard]] bool getDisableSprites() const { return disableSpritesSetting.getBoolean(); }

	/** CmdTiming [real, broken].
	  * This setting is intended for debugging only, not for users. */
	[[nodiscard]] EnumSetting<bool>& getCmdTimingSetting() { return cmdTimingSetting; }

	/** TooFastAccess [real, ignored].
	  * Indicates whether too fast VDP VRAM access should be correctly
	  * emulated (= some accesses are dropped) or ignored (= all accesses
	  * are correctly executed). */
	[[nodiscard]] EnumSetting<bool>& getTooFastAccessSetting() { return tooFastAccessSetting; }

	/** Display deformation (normal, 3d)
	  * ATM this only works when using the SDLGL-PP renderer. */
	[[nodiscard]] DisplayDeform getDisplayDeform() { return displayDeformSetting.getEnum(); }

	/** VSync [on, off]
	 * ATM this only works when using the SDLGL-PP renderer. */
	[[nodiscard]] BooleanSetting& getVSyncSetting() { return vSyncSetting; }

	/** Amount of horizontal stretch.
	  * This number represents the amount of MSX pixels (normal width) that
	  * will be stretched to the complete width of the host window. */
	[[nodiscard]] FloatSetting& getHorizontalStretchSetting() {
		return horizontalStretchSetting;
	}
	[[nodiscard]] float getHorizontalStretch() const {
		return horizontalStretchSetting.getFloat();
	}

	/** The amount of time until the pointer is hidden in the openMSX
	  * window. negative means: no hiding, 0 means immediately. */
	[[nodiscard]] FloatSetting& getPointerHideDelaySetting() {
		return pointerHideDelaySetting;
	}
	[[nodiscard]] float getPointerHideDelay() const {
		return pointerHideDelaySetting.getFloat();
	}

	/** Is black frame interleaving enabled? */
	[[nodiscard]] bool getInterleaveBlackFrame() const {
		return interleaveBlackFrameSetting.getBoolean();
	}

	/** Apply brightness, contrast and gamma transformation on the input
	  * color component. The component is expected to be in the range
	  * [0.0 .. 1.0] but it's not an error if it lays outside of this range.
	  * The return value is guaranteed to lay inside this range.
	  * This method skips the cross-influence of color components on each
	  * other that is controlled by the "color_matrix" setting.
	  */
	[[nodiscard]] float transformComponent(float c) const;

	/** Apply brightness, contrast and gamma transformation on the input
	  * color. The R, G and B component are expected to be in the range
	  * [0.0 .. 1.0] but it's not an error if a component lays outside of
	  * this range. After transformation it's guaranteed all components
	  * lay inside this range.
	  */
	[[nodiscard]] gl::vec3 transformRGB(gl::vec3 rgb) const;

private:
	[[nodiscard]] static EnumSetting<ScaleAlgorithm>::Map getScalerMap();
	[[nodiscard]] static EnumSetting<RendererID>::Map getRendererMap();

	// Observer:
	void update(const Setting& setting) noexcept override;

	/** Sets the "brightness" and "contrast" fields according to the setting
	  * values.
	  */
	void updateBrightnessAndContrast();

	void parseColorMatrix(Interpreter& interp, const TclObject& value);

private:
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
	BooleanSetting vSyncSetting;
	FloatSetting horizontalStretchSetting;
	FloatSetting pointerHideDelaySetting;
	BooleanSetting interleaveBlackFrameSetting;

	float brightness;
	float contrast;

	/** Parsed color matrix, kept in sync with colorMatrix setting. */
	gl::mat3 colorMatrix;
	/** True iff color matrix is identity matrix. */
	bool cmIdentity;
};

} // namespace openmsx

#endif
