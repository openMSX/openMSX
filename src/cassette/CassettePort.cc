// $Id$

#include <memory>
#include "CassettePort.hh"
#include "CassetteDevice.hh"
#include "CassettePlayer.hh"
#include "DummyCassetteDevice.hh"
#include "MSXMotherBoard.hh"
#include "PluggingController.hh"
#include "HardwareConfig.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {

// CassettePortInterface //

CassettePortInterface::CassettePortInterface()
	: Connector("cassetteport", auto_ptr<Pluggable>(new DummyCassetteDevice()))
{
}

void CassettePortInterface::unplug(const EmuTime& time)
{
	flushOutput(time);
	Connector::unplug(time);
}

const string& CassettePortInterface::getDescription() const
{
	static const string desc("MSX Cassette port.");
	return desc;
}

const string &CassettePortInterface::getClass() const
{
	static const string className("Cassette Port");
	return className;
}

CassetteDevice& CassettePortInterface::getPlugged() const
{
	return static_cast<CassetteDevice&>(*plugged);
}


// DummyCassettePort //

DummyCassettePort::DummyCassettePort()
	: CassettePortInterface()
{
}

void DummyCassettePort::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// do nothing
}
void DummyCassettePort::cassetteOut(bool /*output*/, const EmuTime& /*time*/)
{
	// do nothing
}
bool DummyCassettePort::cassetteIn(const EmuTime& /*time*/)
{
	return true;	// TODO check on Turbo-R
}
void DummyCassettePort::flushOutput(const EmuTime& /*time*/)
{
	// do nothing
}


// CassettePort //

CassettePort::CassettePort(MSXMotherBoard& motherBoard)
	: CassettePortInterface()
	, pluggingController(motherBoard.getPluggingController())
{
	cassettePlayer.reset(new CassettePlayer(motherBoard.getMixer()));
	buffer = new short[BUFSIZE];
	pluggingController.registerConnector(this);
	pluggingController.registerPluggable(cassettePlayer.get());
}

CassettePort::~CassettePort()
{
	pluggingController.unregisterPluggable(cassettePlayer.get());
	pluggingController.unregisterConnector(this);
	delete[] buffer;
}


void CassettePort::setMotor(bool status, const EmuTime& time)
{
	//TODO make 'click' sound
	//PRT_DEBUG("CassettePort: motor " << status);
	getPlugged().setMotor(status, time);
}

void CassettePort::cassetteOut(bool output, const EmuTime& /*time*/)
{
	// this implements a VERY rough filter
	//   on a transition the output is 0
	//   everywhere else it is +A or -A
	// this is probably good enough
	//flushOutput(time);
	nextSample = 0;
	lastOutput = output;
}

bool CassettePort::cassetteIn(const EmuTime& time)
{
	// All analog filtering is ignored for now
	//   only important component is DC-removal
	//   we just assume sample has no DC component
	short sample = getPlugged().readSample(time); // read 1 sample
	bool result = (sample >= 0); // comparator
	//PRT_DEBUG("CassettePort:: read " << result);
	return result;
}

void CassettePort::flushOutput(const EmuTime& time)
{
	CassetteDevice& device = getPlugged();
	// can be changed since prev flush
	int sampleRate = device.getWriteSampleRate();
	if (sampleRate == 0) {
		// 99% of the time
		//PRT_DEBUG("Cas: must not generate wave");
		prevTime = time;
		return;
	}
	int samples = (int)((time - prevTime).toDouble() * sampleRate);
	prevTime = time;
	//PRT_DEBUG("Cas: generate " << samples << " samples");

	// dumb implementation, good enough for now
	device.writeWave(&nextSample, 1);
	samples--;
	nextSample = lastOutput ? 32767 : -32768;
	int numSamples = (samples > BUFSIZE) ? BUFSIZE : samples;
	for (int i=1; i<numSamples; i++) buffer[i] = nextSample;
	while (samples) {
		device.writeWave(buffer, numSamples);
		samples -= numSamples;
		numSamples = (samples > BUFSIZE) ? BUFSIZE : samples;
	}
}

} // namespace openmsx
