// $Id: $

// TODO frameskip
//      record gfx9000
//      implement a command to start/stop recording / specify filename

#include "AviRecorder.hh"
#include "AviWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "Display.hh"
#include "PostProcessor.hh"
#include "MSXMixer.hh"
#include "Scheduler.hh"
#include "GlobalCliComm.hh"
#include <cmath>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

AviRecorder::AviRecorder(Reactor& reactor_)
	: SimpleCommand(reactor_.getCommandController(), "record")
	, reactor(reactor_)
	, postProcessor(0)
	, mixer(0)
	, scheduler(0)
{
}

AviRecorder::~AviRecorder()
{
	assert(!writer.get());
	assert(!postProcessor);
	assert(!mixer);
	assert(!scheduler);
}

void AviRecorder::start(const string& filename)
{
	stop();
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		return;
	}
	scheduler = &motherBoard->getScheduler();
	mixer = &motherBoard->getMSXMixer();

	Display& display = reactor.getDisplay();
	Layer* layer = display.findLayer("V99x8 PostProcessor");
	postProcessor = dynamic_cast<PostProcessor*>(layer);
	if (!postProcessor) {
		return;
	}

	postProcessor->setRecorder(this);
	mixer->setRecorder(this);
	unsigned bpp = postProcessor->getBpp();
	frameDuration = postProcessor->getLastFrameDuration();
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
	if (!warnedFps &&
	    (fabs(postProcessor->getLastFrameDuration() - frameDuration) > 1e-5)) {
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
	if (postProcessor) {
		postProcessor->setRecorder(0);
		postProcessor = 0;
	}
	if (mixer) {
		mixer->setRecorder(0);
		mixer = 0;
	}
	scheduler = 0;
	writer.reset();
}


string AviRecorder::execute(const vector<string>& /*tokens*/)
{
	// TODO
	start("test.avi");
	return "";
}

string AviRecorder::help(const vector<string>& /*tokens*/) const
{
	return "TODO";
}

void AviRecorder::tabCompletion(vector<string>& /*tokens*/) const
{
	// TODO
}

} // namespace openmsx
