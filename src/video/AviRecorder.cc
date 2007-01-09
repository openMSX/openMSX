// $Id: $

// TODO merge with soundlog command

#include "AviRecorder.hh"
#include "AviWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "MSXMixer.hh"
#include "Scheduler.hh"
#include "GlobalCliComm.hh"
#include "FileOperations.hh"
#include <cmath>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

AviRecorder::AviRecorder(Reactor& reactor_)
	: SimpleCommand(reactor_.getCommandController(), "record")
	, reactor(reactor_)
	, videoSource(0)
	, postProcessor1(0)
	, postProcessor2(0)
	, mixer(0)
	, scheduler(0)
{
}

AviRecorder::~AviRecorder()
{
	assert(!writer.get());
}

void AviRecorder::start(const string& filename)
{
	stop();
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		throw CommandException("No active MSX machine.");
	}
	scheduler = &motherBoard->getScheduler();
	mixer = &motherBoard->getMSXMixer();

	Display& display = reactor.getDisplay();
	videoSource = &display.getRenderSettings().getVideoSource();
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
	if (postProcessor1) {
		postProcessor1->setRecorder(this);
	}
	if (postProcessor2) {
		postProcessor2->setRecorder(this);
	}
	mixer->setRecorder(this);
	unsigned bpp = activePP->getBpp();
	frameDuration = activePP->getLastFrameDuration();
	sampleRate = mixer->getSampleRate();
	writer.reset(new AviWriter(filename, 320, 240, bpp,
	                           1.0 / frameDuration, sampleRate));

	warnedFps = false;
	warnedSampleRate = false;
}

void AviRecorder::addWave(unsigned num, short* data)
{
	audioBuf.insert(audioBuf.end(), data, data + 2 * num);
}

void AviRecorder::addImage(const void** lines)
{
	if (!warnedSampleRate && mixer &&
	    (mixer->getSampleRate() != sampleRate)) {
		warnedSampleRate = true;
		reactor.getGlobalCliComm().printWarning(
			"Detected audio sample frequency change during "
			"avi recording. Audio/video might get out of sync "
			"because of this.");
	}
	PostProcessor* activePP = videoSource->getValue() == VIDEO_MSX
	                        ? postProcessor1 : postProcessor2;
	if (!warnedFps &&
	    (fabs(activePP->getLastFrameDuration() - frameDuration) > 1e-5)) {
		warnedFps = true;
		reactor.getGlobalCliComm().printWarning(
			"Detected frame rate change (PAL/NTSC or frameskip) "
			"during avi recording. Audio/video might get out of "
			"sync because of this.");
	}
	if (mixer) {
		mixer->updateStream(scheduler->getCurrentTime());
	}
	writer->addFrame(lines, audioBuf.size() / 2, &audioBuf[0]);
	audioBuf.clear();
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
	writer.reset();
}


string AviRecorder::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	if (tokens[1] == "start") {
		return processStart(tokens);
	} else if (tokens[1] == "stop") {
		return processStop(tokens);
	} else if (tokens[1] == "toggle") {
		return processToggle(tokens);
	}
	throw SyntaxError();
}

string AviRecorder::processStart(const vector<string>& tokens)
{
	string filename;
	string prefix = "openmsx";
	switch (tokens.size()) {
	case 2:
		// nothing
		break;
	case 3:
		if (tokens[2] == "-prefix") {
			throw CommandException("Missing argument");
		}
		filename = tokens[2];
		break;
	case 4:
		if (tokens[2] != "-prefix") {
			throw SyntaxError();
		}
		prefix = tokens[3];
		break;
	default:
		throw SyntaxError();
	}
	if (filename.empty()) {
		filename = FileOperations::getNextNumberedFileName(
			"videos", prefix, ".avi");
	}
	
	if (writer.get()) {
		return "Already recording.";
	}
	start(filename);
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
	if (writer.get()) {
		// drop extra tokens
		vector<string> tmp(tokens.begin(), tokens.begin() + 2);
		return processStop(tmp);
	} else {
		return processStart(tokens);
	}
}

string AviRecorder::help(const vector<string>& /*tokens*/) const
{
	return "Controls video recording: write openmsx audio/video to a .avi file.\n"
	       "record start              Record to file 'openmsxNNNN.avi'\n"
	       "record start <filename>   Record to given file\n"
	       "record start -prefix foo  Record to file 'fooNNNN.avi'\n"
	       "record stop               Stop recording\n"
	       "record toggle             Toggle recording (useful as keybinding)\n";
}

void AviRecorder::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		const char* const str[3] = { "start", "stop", "toggle" };
		std::set<string> cmds(str, str + 3);
		completeString(tokens, cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "start")) {
		const char* const str[1] = { "-prefix" };
		std::set<string> cmds(str, str + 1);
		completeString(tokens, cmds);
	}
}

} // namespace openmsx
