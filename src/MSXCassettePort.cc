// $Id$

#include "MSXCassettePort.hh"
#include "DummyCassetteDevice.hh"
#include "MSXCPU.hh"
#include "CassetteDevice.hh"

// MSXCassettePort //

CassettePortInterface *MSXCassettePort::instance()
{
	if (oneInstance == NULL) {
		/*
		// TODO figure out why this doesn't work
		try {
			MSXConfig::instance()->getConfigById("CassettePort");
			// there is a CassettePort in config
			oneInstance = new CassettePort();
		} catch (MSXConfig::Exception e) {
			// there is no CassettePort in config
			oneInstance = new DummyCassettePort();
		}
		*/
		oneInstance = new DummyCassettePort();
	}
	return oneInstance;
}
CassettePortInterface *MSXCassettePort::oneInstance = NULL;



// CassettePortInterface //

void CassettePortInterface::plug(CassetteDevice *dev)
{
	if (device != dev) {
		flushOutput(MSXCPU::instance()->getCurrentTime());
		device = dev;
	}
}

void CassettePortInterface::unplug()
{
	plug(DummyCassetteDevice::instance());
}


// DummyCassettePort //

DummyCassettePort::DummyCassettePort()
{
	unplug();
}
void DummyCassettePort::setMotor(bool status, const EmuTime &time)
{
	// do nothing
}
void DummyCassettePort::cassetteOut(bool output, const EmuTime &time)
{
	// do nothing
}
bool DummyCassettePort::cassetteIn(const EmuTime &time)
{
	return true;	// TODO check on Turbo-R
}
void DummyCassettePort::flushOutput(const EmuTime &time)
{
	// do nothing
}


// CassettePort //

CassettePort::CassettePort()
{
	unplug();
	buffer = new short[BUFSIZE];
}

CassettePort::~CassettePort()
{
	flushOutput(MSXCPU::instance()->getCurrentTime());
	delete[] buffer;
}

void CassettePort::setMotor(bool status, const EmuTime &time)
{
	device->setMotor(status, time);
	//TODO make 'click' sound
}

void CassettePort::cassetteOut(bool output, const EmuTime &time)
{
	// this implements a VERY rough filter
	//   on a transition the output is 0
	//   everywhere else it is max or min
	// this is probably good enough
	flushOutput(time);
	nextSample = 0;
	lastOutput = output;
}

bool CassettePort::cassetteIn(const EmuTime &time)
{
	// All analog filtering is ignored for now
	//   only important component is DC-removal
	//   we just assume sample has no DC component
	short sample = device->readSample(time);	// read 1 sample  
	return (sample > 0); // comparator
}

void CassettePort::flushOutput(const EmuTime &time)
{
	int sampleRate = device->getWriteSampleRate();	// can be changed since prev flush
	if (sampleRate == 0) {
		// 99% of the time
		PRT_DEBUG("Cas: must not generate wave");
		prevTime = time;
		return;
	}
	int samples = (int)(prevTime.getDuration(time) * sampleRate);
	prevTime = time;
	PRT_DEBUG("Cas: generate " << samples << " samples");
	
	// dumb implementation, good enough for now
	device->writeWave(&nextSample, 1);
	samples--;
	nextSample = lastOutput ? 32767 : -32768;
	int numSamples = (samples > BUFSIZE) ? BUFSIZE : samples;
	for (int i=1; i<numSamples; i++) buffer[i] = nextSample;
	while (samples) {
		device->writeWave(buffer, numSamples);
		samples -= numSamples;
		numSamples = (samples > BUFSIZE) ? BUFSIZE : samples;
	}
}
