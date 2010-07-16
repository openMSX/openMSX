// $Id$

#include "AviRecorder.hh"
#include "AviWriter.hh"
#include "WavWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "FileContext.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include "PostProcessor.hh"
#include "MSXMixer.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "FileOperations.hh"
#include "StringOp.hh"
#include "TclObject.hh"
#include "vla.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class RecordCommand : public Command
{
public:
	RecordCommand(CommandController& commandController, AviRecorder& recorder);
	virtual void execute(const vector<TclObject*>& tokens, TclObject& result);
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

void AviRecorder::start(bool recordAudio, bool recordVideo, bool recordMono,
                        bool recordStereo, const Filename& filename)
{
	stop();
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		throw CommandException("No active MSX machine.");
	}
	if (recordAudio) {
		mixer = &motherBoard->getMSXMixer();
		warnedStereo = false;
		if (recordStereo) {
			stereo = true;
		} else if (recordMono) {
			stereo = false;
			warnedStereo = true; // no warning if data is actually stereo
		} else {
			stereo = mixer->needStereoRecording();
		}		
		sampleRate = mixer->getSampleRate();
		warnedSampleRate = false;
	}
	if (recordVideo) {
		Display& display = reactor.getDisplay();
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

		try {
			aviWriter.reset(new AviWriter(filename, frameWidth,
			        frameHeight, bpp,
			        (recordAudio && stereo) ? 2 : 1, sampleRate));
		} catch (MSXException& e) {
			throw CommandException("Can't start recording: " +
			                       e.getMessage());
		}
	} else {
		assert(recordAudio);
		wavWriter.reset(new Wav16Writer(filename, stereo ? 2 : 1,
		                                sampleRate));
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
	if (stereo) {
		if (wavWriter.get()) {
			wavWriter->write(data, 2, num);
		} else {
			assert(aviWriter.get());
			audioBuf.insert(audioBuf.end(), data, data + 2 * num);
		}
	} else {
		VLA(short, buf, num);
		unsigned i = 0;
		for (/**/; !warnedStereo && i < num; ++i) {
			if (data[2 * i + 0] != data[2 * i + 1]) {
				reactor.getCliComm().printWarning(
				    "Detected stereo sound during mono recording. "
				    "Channels will be mixed down to mono. To "
				    "avoid this warning you can explicity pass the "
				    "-mono or -stereo flag to the record command.");
				warnedStereo = true;
				break;
			}
			buf[i] = data[2 * i];
		}
		for (/**/; i < num; ++i) {
			buf[i] = (int(data[2 * i + 0]) + int(data[2 * i + 1])) / 2;
		}

		if (wavWriter.get()) {
			wavWriter->write(buf, 1, num);
		} else {
			assert(aviWriter.get());
			audioBuf.insert(audioBuf.end(), buf, buf + num);
		}
	}
}

void AviRecorder::addImage(FrameSource* frame, EmuTime::param time)
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
		mixer->updateStream(time);
	}
	// TODO vector::data() is not yet supported in gcc-3.4
	short* audioData = audioBuf.empty() ? NULL : &audioBuf[0];
	aviWriter->addFrame(frame, unsigned(audioBuf.size()), audioData);
	audioBuf.clear();
}

// TODO: Can this be dropped?
unsigned AviRecorder::getFrameHeight() const {
	assert (frameHeight != 0); // someone uses the getter too early?
	return frameHeight;
}

void AviRecorder::processStart(const vector<TclObject*>& tokens, TclObject& result)
{
	string filename;
	string prefix = "openmsx";
	bool recordAudio = true;
	bool recordVideo = true;
	bool recordMono = false;
	bool recordStereo = false;
	frameWidth = 320;
	frameHeight = 240;

	vector<string> arguments;
	for (unsigned i = 2; i < tokens.size(); ++i) {
		const string token = tokens[i]->getString();
		if (StringOp::startsWith(token, "-")) {
			if (token == "--") {
				for (vector<TclObject*>::const_iterator it = tokens.begin() + i + 1; it != tokens.end(); ++it) {
					arguments.push_back((*it)->getString());
				}
				break;
			}
			if (token == "-prefix") {
				if (++i == tokens.size()) {
					throw CommandException("Missing argument");
				}
				prefix = tokens[i]->getString();
			} else if (token == "-audioonly") {
				recordVideo = false;
			} else if (token == "-mono") {
				recordMono = true;
			} else if (token == "-stereo") {
				recordStereo = true;
			} else if (token == "-videoonly") {
				recordAudio = false;
			} else if (token == "-doublesize") {
				frameWidth = 640;
				frameHeight = 480;
			} else {
				throw CommandException("Invalid option: " + token);
			}
		} else {
			arguments.push_back(token);
		}
	}
	if (!recordAudio && !recordVideo) {
		throw CommandException("Can't have both -videoonly and -audioonly.");
	}
	if (recordStereo && recordMono) {
		throw CommandException("Can't have both -mono and -stereo.");
	}
	if (!recordAudio && (recordStereo || recordMono)) {
		throw CommandException("Can't have both -videoonly and -stereo or -mono.");
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

	string directory = recordVideo ? "videos" : "soundlogs";
	string extension = recordVideo ? ".avi"   : ".wav";
	if (filename.empty()) {
		filename = FileOperations::getNextNumberedFileName(
			directory, prefix, extension);
	} else {
		if (FileOperations::getExtension(filename).empty()) {
			// no extension given, append standard one
			filename += extension;
		}
		if (FileOperations::getBaseName(filename).empty()) {
			// no dir given, use standard dir
			filename = FileOperations::getUserOpenMSXDir() + "/" + directory + "/" + filename;
		}
	}

	if (aviWriter.get() || wavWriter.get()) {
		result.setString("Already recording.");
	} else {
		start(recordAudio, recordVideo, recordMono, recordStereo,
				Filename(filename));
		result.setString("Recording to " + filename);
	}
}

void AviRecorder::processStop(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	stop();
}

void AviRecorder::processToggle(const vector<TclObject*>& tokens, TclObject& result)
{
	if (aviWriter.get() || wavWriter.get()) {
		// drop extra tokens
		vector<TclObject*> tmp(tokens.begin(), tokens.begin() + 2);
		processStop(tmp);
	} else {
		processStart(tokens, result);
	}
}

void AviRecorder::status(const vector<TclObject*>& tokens, TclObject& result) const
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	result.addListElement("status");
	if (aviWriter.get() || wavWriter.get()) {
		result.addListElement("recording");
	} else {
		result.addListElement("idle");
	}

}

// class RecordCommand

RecordCommand::RecordCommand(CommandController& commandController,
                             AviRecorder& recorder_)
	: Command(commandController, "record")
	, recorder(recorder_)
{
}

void RecordCommand::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	const string subcommand = tokens[1]->getString();
	if (subcommand == "start") {
		recorder.processStart(tokens, result);
	} else if (subcommand == "stop") {
		recorder.processStop(tokens);
	} else if (subcommand == "toggle") {
		recorder.processToggle(tokens, result);
	} else if (subcommand == "status") {
		recorder.status(tokens, result);
	} else {
		throw SyntaxError();
	}
}

string RecordCommand::help(const vector<string>& /*tokens*/) const
{
	return "Controls video recording: Write openMSX audio/video to a .avi file.\n"
	       "record start              Record to file 'openmsxNNNN.avi'\n"
	       "record start <filename>   Record to given file\n"
	       "record start -prefix foo  Record to file 'fooNNNN.avi'\n"
	       "record stop               Stop recording\n"
	       "record toggle             Toggle recording (useful as keybinding)\n"
	       "record status             Query recording state\n"
	       "\n"
	       "The start subcommand also accepts an optional -audioonly, -videoonly, "
	       " -mono, -stereo, -doublesize flag.\n"
	       "Videos are recorded in a 320x240 size by default and at 640x480 when the "
	       "-doublesize flag is used.";
}

void RecordCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		const char* const str[4] = { "start", "stop", "toggle",
			"status" };
		std::set<string> cmds(str, str + 4);
		completeString(tokens, cmds);
	} else if ((tokens.size() >= 3) && (tokens[1] == "start")) {
		const char* const str[6] = { "-prefix", "-videoonly", 
			"-audioonly", "-doublesize", "-mono", "-stereo" };
		std::set<string> cmds(str, str + 6);
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context, cmds);
	}
}

} // namespace openmsx
