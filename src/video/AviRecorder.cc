// $Id: $

#include "AviRecorder.hh"
#include "AviWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "Display.hh"
#include "PostProcessor.hh"
#include "VDP.hh"
#include "MSXMixer.hh"
#include "Scheduler.hh"
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
	VDP* vdp = dynamic_cast<VDP*>(motherBoard->findDevice("VDP"));
	if (!vdp) {
		return;
	}

	postProcessor->setRecorder(this);
	mixer->setRecorder(this);
	// TODO get bpp
	unsigned bpp = postProcessor->getBpp();
	double fps = vdp->isPalTiming()
	           ? (6.0 * 3579545.0) / (1368.0 * 313.0)  // PAL
	           : (6.0 * 3579545.0) / (1368.0 * 262.0); // NTSC
	unsigned sampleRate = mixer->getSampleRate();
	writer.reset(new AviWriter(filename, 320, 240, bpp, fps, sampleRate));
}

void AviRecorder::addWave(unsigned num, short* data)
{
	audioBuf.insert(audioBuf.end(), data, data + 2 * num);
}

void AviRecorder::addImage(const void** lines)
{
	// TODO check fps/samplerate is still the same
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
