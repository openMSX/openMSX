// $Id$

#include "AviRecorder.hh"
#include "AviWriter.hh"
#include "WavWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include "PostProcessor.hh"
#include "MSXMixer.hh"
#include "Scheduler.hh"
#include "CliComm.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class RecordCommand : public SimpleCommand
{
public:
	RecordCommand(CommandController& commandController, AviRecorder& recorder);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	AviRecorder& recorder;
};


AviRecorder::AviRecorder(Reactor& reactor_)
	: reactor(reactor_)
	, recordCommand(new RecordCommand(reactor.getCommandController(), *this))
	, postProcessor1(0)
	, postProcessor2(0)
	, mixer(0)
	, scheduler(0)
	, duration(EmuDuration::infinity)
	, prevTime(EmuTime::infinity)
	, frameHeight(0)
{
}

AviRecorder::~AviRecorder()
{
	assert(!aviWriter.get());
	assert(!wavWriter.get());
}

void AviRecorder::start(bool recordAudio, bool recordVideo,
                        const string& filename)
{
	stop();
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		throw CommandException("No active MSX machine.");
	}
	if (recordAudio) {
		mixer = &motherBoard->getMSXMixer();
		sampleRate = mixer->getSampleRate();
		warnedSampleRate = false;
	}
	if (recordVideo) {
		Display& display = reactor.getDisplay();
		scheduler = &motherBoard->getScheduler();
		VideoSourceSetting* videoSource =
			&display.getRenderSettings().getVideoSource();
		Layer* layer1 = display.findLayer("V99x8 PostProcessor");
		Layer* layer2 = display.findLayer("V9990 PostProcessor");
		postProcessor1 = dynamic_cast<PostProcessor*>(layer1);
		postProcessor2 = dynamic_cast<PostProcessor*>(layer2);
		PostProcessor* activePP = videoSource->getValue() == VIDEO_MSX
					? postProcessor1 : postProcessor2;
		if (!activePP) {
			throw CommandException(
				"Current renderer doesn't support video recording.");
		}
		unsigned bpp = activePP->getBpp();
		warnedFps = false;
		duration = EmuDuration::infinity;
		prevTime = EmuTime::infinity;

		aviWriter.reset(new AviWriter(filename, frameWidth, frameHeight,
		                              bpp, sampleRate));
	} else {
		assert(recordAudio);
		wavWriter.reset(new WavWriter(filename, 2, 16, sampleRate));
	}
	// only set recorders when all errors are checked for
	if (postProcessor1) {
		postProcessor1->setRecorder(this);
	}
	if (postProcessor2) {
		postProcessor2->setRecorder(this);
	}
	if (mixer) {
		mixer->setRecorder(this);
	}
}

void AviRecorder::stop()
{
	if (postProcessor1) {
		postProcessor1->setRecorder(0);
		postProcessor1 = 0;
	}
	if (postProcessor2) {
		postProcessor2->setRecorder(0);
		postProcessor2 = 0;
	}
	if (mixer) {
		mixer->setRecorder(0);
		mixer = 0;
	}
	scheduler = 0;
	sampleRate = 0;
	aviWriter.reset();
	wavWriter.reset();
}

void AviRecorder::addWave(unsigned num, short* data)
{
	if (!warnedSampleRate && (mixer->getSampleRate() != sampleRate)) {
		warnedSampleRate = true;
		reactor.getCliComm().printWarning(
			"Detected audio sample frequency change during "
			"avi recording. Audio/video might get out of sync "
			"because of this.");
	}
	if (wavWriter.get()) {
		wavWriter->write16stereo(data, num);
	} else {
		assert(aviWriter.get());
		audioBuf.insert(audioBuf.end(), data, data + 2 * num);
	}
}

void AviRecorder::addImage(const void** lines, const EmuTime& time)
{
	assert(!wavWriter.get());
	if (duration != EmuDuration::infinity) {
		if (!warnedFps && ((time - prevTime) != duration)) {
			warnedFps = true;
			reactor.getCliComm().printWarning(
				"Detected frame rate change (PAL/NTSC or frameskip) "
				"during avi recording. Audio/video might get out of "
				"sync because of this.");
		}
	} else if (prevTime != EmuTime::infinity) {
		duration = time - prevTime;
		aviWriter->setFps(1.0 / duration.toDouble());
	}
	prevTime = time;

	if (mixer) {
		mixer->updateStream(scheduler->getCurrentTime());
	}
	aviWriter->addFrame(lines, audioBuf.size() / 2, &audioBuf[0]);
	audioBuf.clear();
}

unsigned AviRecorder::getFrameHeight() const {
	assert (frameHeight != 0); // someone uses the getter too early?
	return frameHeight;
}

string AviRecorder::processStart(const vector<string>& tokens)
{
	string filename;
	string prefix = "openmsx";
	bool recordAudio = true;
	bool recordVideo = true;
	frameWidth = 320;
	frameHeight = 240;

	vector<string> arguments;
	for (unsigned i = 2; i < tokens.size(); ++i) {
		if (StringOp::startsWith(tokens[i], "-")) {
			if (tokens[i] == "--") {
				arguments.insert(arguments.end(),
					tokens.begin() + i + 1, tokens.end());
				break;
			}
			if (tokens[i] == "-prefix") {
				if (++i == tokens.size()) {
					throw CommandException("Missing argument");
				}
				prefix = tokens[i];
			} else if (tokens[i] == "-audioonly") {
				recordVideo = false;
			} else if (tokens[i] == "-videoonly") {
				recordAudio = false;
			} else if (tokens[i] == "-doublesize") {
				frameWidth = 640;
				frameHeight = 480;
			} else {
				throw CommandException("Invalid option");
			}
		} else {
			arguments.push_back(tokens[i]);
		}
	}
	if (!recordAudio && !recordVideo) {
		throw CommandException("Can't have both -videoonly and -audioonly.");
	}
	switch (arguments.size()) {
	case 0:
		// nothing
		break;
	case 1:
		filename = arguments[0];
		break;
	default:
		throw SyntaxError();
	}

	if (filename.empty()) {
		string directory = recordVideo ? "videos" : "soundlogs";
		string extension = recordVideo ? ".avi"   : ".wav";
		filename = FileOperations::getNextNumberedFileName(
			directory, prefix, extension);
	}

	if (aviWriter.get() || wavWriter.get()) {
		return "Already recording.";
	}
	start(recordAudio, recordVideo, filename);
	return "Recording to " + filename;
}

string AviRecorder::processStop(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	stop();
	return "";
}

string AviRecorder::processToggle(const vector<string>& tokens)
{
	if (aviWriter.get() || wavWriter.get()) {
		// drop extra tokens
		vector<string> tmp(tokens.begin(), tokens.begin() + 2);
		return processStop(tmp);
	} else {
		return processStart(tokens);
	}
}


// class RecordCommand

RecordCommand::RecordCommand(CommandController& commandController,
                             AviRecorder& recorder_)
	: SimpleCommand(commandController, "record")
	, recorder(recorder_)
{
}

string RecordCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "start") {
		return recorder.processStart(tokens);
	} else if (tokens[1] == "stop") {
		return recorder.processStop(tokens);
	} else if (tokens[1] == "toggle") {
		return recorder.processToggle(tokens);
	}
	throw SyntaxError();
}

string RecordCommand::help(const vector<string>& /*tokens*/) const
{
	return "Controls video recording: write openmsx audio/video to a .avi file.\n"
	       "record start              Record to file 'openmsxNNNN.avi'\n"
	       "record start <filename>   Record to given file\n"
	       "record start -prefix foo  Record to file 'fooNNNN.avi'\n"
	       "record stop               Stop recording\n"
	       "record toggle             Toggle recording (useful as keybinding)\n"
	       "\n"
	       "The start subcommand also accepts an optional -audioonly, -videoonly and "
	       "a -doublesize flag.\n"
	       "Videos are recorded in a 320x240 size by default and at 640x480 when the "
		   "-doublesize flag is used.";
}

void RecordCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		const char* const str[3] = { "start", "stop", "toggle" };
		std::set<string> cmds(str, str + 3);
		completeString(tokens, cmds);
	} else if ((tokens.size() >= 3) && (tokens[1] == "start")) {
		const char* const str[4] = { "-prefix", "-videoonly", "-audioonly",
						"-doublesize"};
		std::set<string> cmds(str, str + 4);
		completeString(tokens, cmds);
	}
}

} // namespace openmsx
