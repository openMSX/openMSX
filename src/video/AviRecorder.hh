#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "Command.hh"
#include "EmuTime.hh"
#include <cstdint>
#include <span>
#include <vector>
#include <memory>

namespace openmsx {

class AviWriter;
class Filename;
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
	explicit AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(unsigned num, float* data);
	void addImage(FrameSource* frame, EmuTime::param time);
	void stop();
	[[nodiscard]] unsigned getFrameHeight() const;

private:
	void start(bool recordAudio, bool recordVideo, bool recordMono,
		   bool recordStereo, const Filename& filename);
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
	MSXMixer* mixer;
	EmuDuration duration;
	EmuTime prevTime;
	unsigned sampleRate;
	unsigned frameWidth;
	unsigned frameHeight;
	bool warnedFps;
	bool warnedSampleRate;
	bool warnedStereo;
	bool stereo;
};

} // namespace openmsx

#endif
