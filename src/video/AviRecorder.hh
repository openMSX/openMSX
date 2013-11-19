#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
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
class RecordCommand;
class TclObject;

class AviRecorder : private noncopyable
{
public:
	explicit AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(unsigned num, short* data);
	void addImage(FrameSource* frame, EmuTime::param time);
	void stop();
	unsigned getFrameHeight() const;

private:
	void start(bool recordAudio, bool recordVideo, bool recordMono,
		   bool recordStereo, const Filename& filename);
	void status(const std::vector<TclObject>& tokens, TclObject& result) const;

	void processStart(const std::vector<TclObject>& tokens, TclObject& result);
	void processStop(const std::vector<TclObject>& tokens);
	void processToggle(const std::vector<TclObject>& tokens, TclObject& result);

	Reactor& reactor;
	const std::unique_ptr<RecordCommand> recordCommand;
	std::vector<short> audioBuf;
	std::unique_ptr<AviWriter> aviWriter;
	std::unique_ptr<Wav16Writer> wavWriter;
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

	friend class RecordCommand;
};

} // namespace openmsx

#endif
