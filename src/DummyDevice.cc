
#include "DummyDevice.hh"
#include <assert.h>


DummyDevice::DummyDevice()
{
	PRT_DEBUG ("Instantiating dummy device");
	deviceName = new string("Dummy device");
};

DummyDevice::~DummyDevice()
{
	PRT_DEBUG ("Destroying dummy device");
};

DummyDevice* DummyDevice::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new DummyDevice();
	}
	return oneInstance;
}

//static MSXDevice* DummyDevice::instantiate()
//{
//	assert(false);
//};

void DummyDevice::setConfigDevice(MSXConfig::Device *config)
{
	assert(false);
};

void DummyDevice::init()
{
	assert(false);
};

void DummyDevice::start()
{
	assert(false);
};

void DummyDevice::stop()
{
	assert(false);
};

void DummyDevice::reset()
{
	assert(false);
};

void DummyDevice::saveState(ofstream &writestream)
{
	assert(false);
};

void DummyDevice::restoreState(string &devicestring, ifstream &readstream)
{
	assert(false);
};

DummyDevice *DummyDevice::oneInstance;

