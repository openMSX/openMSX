// $Id: MSXCASSETTE.cc,v

#include "MSXCassettePort.hh"
#include "DummyCassetteDevice.hh"


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
	device = dev;
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
void DummyCassettePort::flushOutput()
{
	// do nothing
}


// CassettePort //

//TODO implement filters and interact with CassettePlayer object

CassettePort::~CassettePort()
{
	flushOutput();
}
void CassettePort::setMotor(bool status, const Emutime &time)
{
	device->setMotor(status, time);
}
void CassettePort::cassetteOut(bool output, const Emutime &time)
{
	// TODO implement low pass filter and buffer data
}
bool CassettePort::cassetteIn(const Emutime &time)
{
	// TODO implement band pass filter 
	short buffer[1];
	device->readWave(buffer, 1, time);
	return (buffer[0]>0); // comparator
}
void CassettePort::flushOutput()
{
	//TODO flush all buffered data
}
