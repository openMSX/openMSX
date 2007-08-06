// $Id$

#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "EmuTime.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Reactor;
class VideoSourceSetting;
class AviWriter;
class WavWriter;
class PostProcessor;
class MSXMixer;
class Scheduler;
class RecordCommand;

class AviRecorder
{
public:
	AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(unsigned num, short* data);
	void addImage(const void** lines, const EmuTime& time);
	void stop();

private:
	void start(bool recordAudio, bool recordVideo,
	           const std::string& filename);

	std::string processStart(const std::vector<std::string>& tokens);
	std::string processStop(const std::vector<std::string>& tokens);
	std::string processToggle(const std::vector<std::string>& tokens);

	Reactor& reactor;
	const std::auto_ptr<RecordCommand> recordCommand;
	std::vector<short> audioBuf;
	std::auto_ptr<AviWriter> aviWriter;
	std::auto_ptr<WavWriter> wavWriter;
	PostProcessor* postProcessor1;
	PostProcessor* postProcessor2;
	MSXMixer* mixer;
	Scheduler* scheduler;
	double frameDuration;
	EmuDuration duration;
	EmuTime prevTime;
	unsigned sampleRate;
	bool warnedFps;
	bool warnedSampleRate;

	friend class RecordCommand;
};

} // namespace openmsx

#endif
