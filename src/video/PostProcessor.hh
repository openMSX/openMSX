#ifndef GLPOSTPROCESSOR_HH
#define GLPOSTPROCESSOR_HH

#include "GLUtil.hh"
#include "RenderSettings.hh"
#include "VideoLayer.hh"

#include "EmuTime.hh"
#include "Schedulable.hh"

#include <array>
#include <memory>
#include <vector>

namespace openmsx {

class AviRecorder;
class CliComm;
class Deflicker;
class DeinterlacedFrame;
class Display;
class DoubledFrame;
class EventDistributor;
class FrameSource;
class GLScaler;
class MSXMotherBoard;
class RawFrame;
class RenderSettings;
class SuperImposedFrame;

/** A post processor builds the frame that is displayed from the MSX frame,
  * while applying effects such as scalers, noise etc.
  */
class PostProcessor final : public VideoLayer, private Schedulable
{
public:
	PostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);
	~PostProcessor() override;

	// Layer interface:
	void paint(OutputSurface& output) override;

	/** Sets up the "abcdFrame" variables for a new frame.
	  * TODO: The point of passing the finished frame in and the new workFrame
	  *       out is to be able to split off the scaler application as a
	  *       separate class.
	  * @param finishedFrame Frame that has just become available.
	  * @param time The moment in time the frame becomes available. Used to
	  *             calculate the framerate for recording (depends on
	  *             PAL/NTSC, frameskip).
	  * @return RawFrame object that can be used for building the next frame.
	  */
	[[nodiscard]] std::unique_ptr<RawFrame> rotateFrames(
		std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time);

	/** Set the Video frame on which to superimpose the 'normal' output of
	  * this PostProcessor. Superimpose is done (preferably) after the
	  * normal output is scaled. IOW the video frame is (preferably) left
	  * unchanged, though exceptions are e.g. scalers that render
	  * scanlines, those are preferably also visible in the video frame.
	  */
	void setSuperimposeVideoFrame(const RawFrame* videoSource) {
		superImposeVideoFrame = videoSource;
	}

	/** Set the VDP frame on which to superimpose the 'normal' output of
	  * this PostProcessor. This is similar to the method above, except
	  * that now the superimposing is done before scaling. IOW both frames
	  * get scaled.
	  */
	void setSuperimposeVdpFrame(const FrameSource* vdpSource) {
		superImposeVdpFrame = vdpSource;
	}

	/** Start/stop recording.
	  * @param recorder_ Finished frames should be pushed to this
	  *                  AviRecorder. Can also be nullptr, meaning
	  *                  recording is stopped.
	  */
	void setRecorder(AviRecorder* recorder_) { recorder = recorder_; }

	/** Is recording active.
	  * ATM used to keep frameskip constant during recording.
	  */
	[[nodiscard]] bool isRecording() const { return recorder != nullptr; }

	/** Get the frame that would be displayed. E.g. so that it can be
	  * superimposed over the output of another PostProcessor, see
	  * setSuperimposeVdpFrame().
	  */
	[[nodiscard]] FrameSource* getPaintFrame() const { return paintFrame; }

	// VideoLayer
	void takeRawScreenShot(unsigned height, const std::string& filename) override;

	[[nodiscard]] CliComm& getCliComm();

private:
	// Observer<Setting> interface:
	void update(const Setting& setting) noexcept override;

	// Schedulable
	void executeUntil(EmuTime::param time) override;

	/** Returns the maximum width for lines [y..y+step).
	  */
	[[nodiscard]] static unsigned getLineWidth(FrameSource* frame, unsigned y, unsigned step);

	void initBuffers();
	void createRegions();
	void uploadFrame();
	void uploadBlock(unsigned srcStartY, unsigned srcEndY,
	                 unsigned lineWidth);

	void preCalcNoise(float factor);
	void drawNoise();
	void drawGlow(int glow);

	void preCalcMonitor3D(float width);
	void drawMonitor3D() const;

private:
	Display& display;
	RenderSettings& renderSettings;
	EventDistributor& eventDistributor;

	/** The surface which is visible to the user. */
	OutputSurface& screen;

	/** The last 4 fully rendered (unscaled) MSX frames. */
	std::array<std::unique_ptr<RawFrame>, 4> lastFrames;

	/** Combined the last two frames in a deinterlaced frame. */
	std::unique_ptr<DeinterlacedFrame> deinterlacedFrame;

	/** Each line of the last frame twice, to get double vertical resolution. */
	std::unique_ptr<DoubledFrame> interlacedFrame;

	/** Combine the last 4 frames into one 'flicker-free' frame. */
	std::unique_ptr<Deflicker> deflicker;

	/** Result of superimposing 2 frames. */
	std::unique_ptr<SuperImposedFrame> superImposedFrame;

	/** Represents a frame as it should be displayed.
	  * This can be simply a RawFrame or two RawFrames combined in a
	  * DeinterlacedFrame or DoubledFrame.
	  */
	FrameSource* paintFrame = nullptr;

	/** Video recorder, nullptr when not recording. */
	AviRecorder* recorder = nullptr;

	/** Video frame on which to superimpose the (VDP) output.
	  * nullptr when not superimposing. */
	const RawFrame* superImposeVideoFrame = nullptr;
	const FrameSource* superImposeVdpFrame = nullptr;

	int interleaveCount = 0; // for interleave-black-frame
	int lastFramesCount = 0; // How many items in lastFrames[] are up-to-date
	unsigned maxWidth; // we lazily create RawFrame objects in lastFrames[]
	unsigned height;   // these two vars remember how big those should be

	/** Laserdisc cannot do interlace (better: the current implementation
	  * is not interlaced). In that case some internal stuff can be done
	  * with less buffers.
	  */
	const bool canDoInterlace;

	EmuTime lastRotate;
	/** The currently active scaler.
	  */
	std::unique_ptr<GLScaler> currScaler;

	struct StoredFrame {
		gl::ivec2 size; // (re)allocate when window size changes
		gl::Texture tex;
		gl::FrameBufferObject fbo;
	};
	std::array<StoredFrame, 2> renderedFrames;

	// Noise effect:
	gl::Texture noiseTextureA{true, true}; // interpolate + wrap
	gl::Texture noiseTextureB{true, true};
	float noiseX = 0.0f, noiseY = 0.0f;

	struct TextureData {
		gl::ColorTexture tex;
		gl::PixelBuffer<unsigned> pbo;
		[[nodiscard]] unsigned width() const { return tex.getWidth(); }
	};
	std::vector<TextureData> textures;

	gl::ColorTexture superImposeTex;

	struct Region {
		Region(unsigned srcStartY_, unsigned srcEndY_,
		       unsigned dstStartY_, unsigned dstEndY_,
		       unsigned lineWidth_)
			: srcStartY(srcStartY_)
			, srcEndY(srcEndY_)
			, dstStartY(dstStartY_)
			, dstEndY(dstEndY_)
			, lineWidth(lineWidth_) {}
		unsigned srcStartY;
		unsigned srcEndY;
		unsigned dstStartY;
		unsigned dstEndY;
		unsigned lineWidth;
	};
	std::vector<Region> regions;

	unsigned frameCounter = 0;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm = RenderSettings::NO_SCALER;

	gl::ShaderProgram monitor3DProg;
	gl::BufferObject arrayBuffer;
	gl::BufferObject elementBuffer;
	gl::BufferObject vbo;
	gl::BufferObject stretchVBO;

	bool storedFrame = false;
};

} // namespace openmsx

#endif // GLPOSTPROCESSOR_HH
