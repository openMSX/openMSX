#ifndef POSTPROCESSOR_HH
#define POSTPROCESSOR_HH

#include "VideoLayer.hh"
#include "Schedulable.hh"
#include "EmuTime.hh"
#include <array>
#include <memory>

namespace openmsx {

class AviRecorder;
class CliComm;
class Deflicker;
class DeinterlacedFrame;
class Display;
class DoubledFrame;
class EventDistributor;
class FrameSource;
class RawFrame;
class RenderSettings;
class SuperImposedFrame;

/** Abstract base class for post processors.
  * A post processor builds the frame that is displayed from the MSX frame,
  * while applying effects such as scalers, noise etc.
  * TODO: With some refactoring, it would be possible to move much or even all
  *       of the post processing code here instead of in the subclasses.
  */
class PostProcessor : public VideoLayer, private Schedulable
{
public:
	~PostProcessor() override;

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
	[[nodiscard]] virtual std::unique_ptr<RawFrame> rotateFrames(
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

	/** Get the number of bits per pixel for the pixels in these frames.
	  * @return Possible values are 15, 16 or 32
	  */
	[[nodiscard]] unsigned getBpp() const;

	/** Get the frame that would be displayed. E.g. so that it can be
	  * superimposed over the output of another PostProcessor, see
	  * setSuperimposeVdpFrame().
	  */
	[[nodiscard]] FrameSource* getPaintFrame() const { return paintFrame; }

	// VideoLayer
	void takeRawScreenShot(unsigned height, const std::string& filename) override;

	[[nodiscard]] CliComm& getCliComm();

protected:
	/** Returns the maximum width for lines [y..y+step).
	  */
	[[nodiscard]] static unsigned getLineWidth(FrameSource* frame, unsigned y, unsigned step);

	PostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);

protected:
	/** Render settings */
	RenderSettings& renderSettings;

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

private:
	// Schedulable
	void executeUntil(EmuTime::param time) override;

private:
	Display& display;

	/** Laserdisc cannot do interlace (better: the current implementation
	  * is not interlaced). In that case some internal stuff can be done
	  * with less buffers.
	  */
	const bool canDoInterlace;

	EmuTime lastRotate;
	EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif // POSTPROCESSOR_HH
