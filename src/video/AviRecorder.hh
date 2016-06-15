#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "Command.hh"
#include "EmuTime.hh"
#include "array_ref.hh"
#include <cstdint>
#include <vector>
#include <memory>

namespace openmsx {

class Reactor;
class AviWriter;
class Wav16Writer;
class Filename;
class PostProcessor;
class FrameSource;
class MSXMixer;
class TclObject;

class AviRecorder
{
public:
	explicit AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(unsigned num, int16_t* data);
	void addImage(FrameSource* frame, EmuTime::param time);
	void stop();
	unsigned getFrameHeight() const;

private:
	void start(bool recordAudio, bool recordVideo, bool recordMono,
		   bool recordStereo, const Filename& filename);
	void status(array_ref<TclObject> tokens, TclObject& result) const;

	void processStart (array_ref<TclObject> tokens, TclObject& result);
	void processStop  (array_ref<TclObject> tokens);
	void processToggle(array_ref<TclObject> tokens, TclObject& result);

	Reactor& reactor;

	struct Cmd final : Command {
		explicit Cmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
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
