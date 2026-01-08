#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "Command.hh"
#include "EmuDuration.hh"
#include "EmuTime.hh"
#include "Mixer.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class AviWriter;
class FrameSource;
class Interpreter;
class MSXMixer;
class PostProcessor;
class Reactor;
class TclObject;
class Wav16Writer;

class AviRecorder
{
public:
	static constexpr std::string_view VIDEO_DIR = "videos";
	static constexpr std::string_view AUDIO_DIR = "soundlogs";
	static constexpr std::string_view VIDEO_EXTENSION = ".avi";
	static constexpr std::string_view AUDIO_EXTENSION = ".wav";

public:
	explicit AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(std::span<const StereoFloat> data);
	void addImage(const FrameSource* frame, EmuTime time);
	void stop();
	[[nodiscard]] unsigned getFrameHeight() const;
	[[nodiscard]] bool isRecording() const;

private:
	void start(bool recordAudio, bool recordVideo, bool recordMono,
		   bool recordStereo, const std::string& filename);
	void status(std::span<const TclObject> tokens, TclObject& result) const;

	void processStart (Interpreter& interp, std::span<const TclObject> tokens, TclObject& result);
	void processStop  (std::span<const TclObject> tokens);
	void processToggle(Interpreter& interp, std::span<const TclObject> tokens, TclObject& result);

private:
	Reactor& reactor;

	struct Cmd final : Command {
		explicit Cmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} recordCommand;

	std::vector<int16_t> audioBuf;
	std::unique_ptr<AviWriter>   aviWriter; // can be nullptr
	std::unique_ptr<Wav16Writer> wavWriter; // can be nullptr
	std::vector<PostProcessor*> postProcessors;
	MSXMixer* mixer = nullptr;
	EmuDuration duration = EmuDuration::infinity();
	EmuTime prevTime = EmuTime::infinity();
	unsigned sampleRate;
	unsigned frameWidth;
	unsigned frameHeight = 0;
	bool warnedFps;
	bool warnedSampleRate;
	bool warnedStereo;
	bool stereo;
};

} // namespace openmsx

#endif
