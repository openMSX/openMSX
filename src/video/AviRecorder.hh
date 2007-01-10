// $Id: $

#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "Command.hh"
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

class AviRecorder : private SimpleCommand
{
public:
	AviRecorder(Reactor& reactor);
	~AviRecorder();

	void addWave(unsigned num, short* data);
	void addImage(const void** lines);
	void stop();

private:
	void start(bool recordAudio, bool recordVideo,
	           const std::string& filename);

	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	std::string processStart(const std::vector<std::string>& tokens);
	std::string processStop(const std::vector<std::string>& tokens);
	std::string processToggle(const std::vector<std::string>& tokens);

	Reactor& reactor;
	VideoSourceSetting* videoSource;
	std::vector<short> audioBuf;
	std::auto_ptr<AviWriter> aviWriter;
	std::auto_ptr<WavWriter> wavWriter;
	PostProcessor* postProcessor1;
	PostProcessor* postProcessor2;
	MSXMixer* mixer;
	Scheduler* scheduler;
	double frameDuration;
	unsigned sampleRate;
	bool warnedFps;
	bool warnedSampleRate;
};

} // namespace openmsx

#endif
