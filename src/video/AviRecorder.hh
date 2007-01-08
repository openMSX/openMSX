// $Id: $

#ifndef AVIRECORDER_HH
#define AVIRECORDER_HH

#include "Command.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class Reactor;
class AviWriter;
class PostProcessor;
class MSXMixer;
class Scheduler;

class AviRecorder : private SimpleCommand
{
public:
	AviRecorder(Reactor& reactor);
	~AviRecorder();
	void start(const std::string& filename);

	void addWave(unsigned num, short* data);
	void addImage(const void** lines);
	void stop();

private:
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

	Reactor& reactor;
	std::vector<short> audioBuf;
	std::auto_ptr<AviWriter> writer;
	PostProcessor* postProcessor;
	MSXMixer* mixer;
	Scheduler* scheduler;
	double frameDuration;
	unsigned sampleRate;
	bool warnedFps;
	bool warnedSampleRate;
};

} // namespace openmsx

#endif
