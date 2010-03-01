// $Id$

#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Reactor;
class VideoSourceSetting;
class AviWriter;
class Wav16Writer;
class Filename;
class PostProcessor;
class FrameSource;
class MSXMixer;
class RecordCommand;

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

	std::string processStart(const std::vector<std::string>& tokens);
	std::string processStop(const std::vector<std::string>& tokens);
	std::string processToggle(const std::vector<std::string>& tokens);

	Reactor& reactor;
	const std::auto_ptr<RecordCommand> recordCommand;
	std::vector<short> audioBuf;
	std::auto_ptr<AviWriter> aviWriter;
	std::auto_ptr<Wav16Writer> wavWriter;
	PostProcessor* postProcessor1;
	PostProcessor* postProcessor2;
	MSXMixer* mixer;
	double frameDuration;
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
