#ifndef FBPOSTPROCESSOR_HH
#define FBPOSTPROCESSOR_HH

#include "MemBuffer.hh"
#include "PixelOperations.hh"
#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "ScalerOutput.hh"
#include <concepts>
#include <span>

namespace openmsx {

class MSXMotherBoard;
class Display;
template<std::unsigned_integral Pixel> class Scaler;

/** Rasterizer using SDL.
  */
template<std::unsigned_integral Pixel>
class FBPostProcessor final : public PostProcessor
{
public:
	FBPostProcessor(
		MSXMotherBoard& motherBoard, Display& display,
		OutputSurface& screen, const std::string& videoSource,
		unsigned maxWidth, unsigned height, bool canDoInterlace);
	~FBPostProcessor() override;
	FBPostProcessor(const FBPostProcessor&) = delete;
	FBPostProcessor& operator=(const FBPostProcessor&) = delete;

	// Layer interface:
	void paint(OutputSurface& output) override;

	[[nodiscard]] std::unique_ptr<RawFrame> rotateFrames(
		std::unique_ptr<RawFrame> finishedFrame, EmuTime::param time) override;

private:
	void preCalcNoise(float factor);
	void drawNoise(OutputSurface& output);
	void drawNoiseLine(std::span<Pixel> buf, signed char* noise);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	/** The currently active scaler.
	  */
	std::unique_ptr<Scaler<Pixel>> currScaler;

	/** The currently active stretch-scaler (horizontal stretch setting).
	 */
	std::unique_ptr<ScalerOutput<Pixel>> stretchScaler;

	/** Currently active scale algorithm, used to detect scaler changes.
	  */
	RenderSettings::ScaleAlgorithm scaleAlgorithm = RenderSettings::NO_SCALER;

	/** Currently active scale factor, used to detect scaler changes.
	  */
	unsigned scaleFactor = unsigned(-1);

	/** Currently active stretch factor, used to detect setting changes.
	  */
	unsigned stretchWidth = unsigned(-1);

	/** Last used output, need to recreate 'stretchScaler' when this changes.
	  */
	OutputSurface* lastOutput = nullptr;

	/** Remember the noise values to get a stable image when paused.
	 */
	MemBuffer<uint16_t> noiseShift;

	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif // FBPOSTPROCESSOR_HH
