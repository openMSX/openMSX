// $Id: MSXCASSETTE.cc,v

#include "MSXCassettePort.hh"
#include "DummyCassetteDevice.hh"
#include "MSXCPU.hh"


// MSXCassettePort //

MSXCassettePort::MSXCassettePort(MSXConfig::Device *config)
{
	constructed = true;
}
bool MSXCassettePort::constructed = false;

MSXCassettePort::~MSXCassettePort()
{
	delete oneInstance;
}

CassettePortInterface *MSXCassettePort::instance()
{
	if (oneInstance == NULL) {
		if (constructed) {
			oneInstance = new CassettePort();
		} else {
			oneInstance = new DummyCassettePort();
		}
	}
	return oneInstance;
}
CassettePortInterface *MSXCassettePort::oneInstance = NULL;



// CassettePortInterface //

CassettePortInterface::CassettePortInterface()
{
	unplug();	// TODO put in config
}

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

void DummyCassettePort::setMotor(bool status, const Emutime &time)
{
	// do nothing
}
void DummyCassettePort::cassetteOut(bool output, const Emutime &time)
{
	// do nothing
}
bool DummyCassettePort::cassetteIn(const Emutime &time)
{
	return true;	// TODO check on Turbo-R
}
void DummyCassettePort::flushOutput(const Emutime &time)
{
	// do nothing
}


// CassettePort //

CassettePort::CassettePort()
{
	buffer = new short[BUFSIZE];
}

CassettePort::~CassettePort()
{
	flushOutput(MSXCPU::instance()->getCurrentTime());
	delete[] buffer;
}

void CassettePort::setMotor(bool status, const Emutime &time)
{
	device->setMotor(status, time);
	//TODO make 'click' sound
}

void CassettePort::cassetteOut(bool output, const Emutime &time)
{
	// this implements a VERY rough filter
	//   on a transition the output is 0
	//   everywhere else it is max or min
	// this is probably good enough
	flushOutput(time);
	nextSample = 0;
	lastOutput = output;
}

bool CassettePort::cassetteIn(const Emutime &time)
{
	// All analog filtering is ignored for now
	//   only important component is DC-removal
	//   we just assume sample has no DC component
	short sample = device->readSample(time);	// read 1 sample  
	return (sample > 0); // comparator
}

void CassettePort::flushOutput(const Emutime &time)
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
