// $Id$
// 
#include "MSXDevice.hh"
#include "iostream.h"

MSXDevice::MSXDevice(void)
{
	PRT_DEBUG("instantiating an MSXDevice object");
};

MSXDevice::~MSXDevice(void)
{
	PRT_DEBUG("destructing an MSXDevice object");
};
//MSXDevice* MSXDevice::instantiate(void) 
//{
//	PRT_ERROR("Something is wrong here\nNobody should be calling MSXDevice::instantiate()");
//	//return new MSXDevice();
//};

void MSXDevice::setConfigDevice(MSXConfig::Device *config)
{
	deviceConfig=config;
	deviceName=&config->getId();
};

// interaction with CPU
byte MSXDevice::readMem(word address, Emutime &time)
{
	return (byte)255;
}
void MSXDevice::writeMem(word address, byte value, Emutime &time)
{
	return;
}
byte MSXDevice:: readIO(byte port, Emutime &time)
{
	return (byte)255;
}
void MSXDevice::writeIO(byte port, byte value, Emutime &time)
{
	return;
}
void MSXDevice::executeUntilEmuTime(const Emutime &time)
{
    return;
}

//int MSXDevice::executeTStates(int TStates)
//{
//	return (int)0;
//}

//int MSXDevice::getUsedTStates(void)
//{
//	return (int)0;
//}



void MSXDevice::init()
{
	PRT_DEBUG ("Initializing " << getName());
}
void MSXDevice::start()
{
	PRT_DEBUG ("Starting " << getName());
	// default implementation same as reset
	reset();
}
void MSXDevice::stop()
{
	PRT_DEBUG ("Stopping " << getName());
}
void MSXDevice::reset()
{
	PRT_DEBUG ("Resetting " << getName());
}
//
void MSXDevice::saveState(ofstream &writestream)
{
	return;
}
void MSXDevice::restoreState(string &devicestring, ifstream &readstream)
{
	return;
}

/*
void MSXDevice::setParameter(string &param, string &valuelist)
{
	return;
}
const string &MSXDevice::getParameter(string &param)
{
	// TODO
}
int MSXDevice::getNrParameters()
{
	return 0;
}
const string &MSXDevice::getParameterTxt(int nr)
{
	// TODO
}
const string &MSXDevice::getParamShortHelp(int nr)
{
	// TODO
}
const string &MSXDevice::getParamLongHelp(int nr)
{
	// TODO
}
*/


const string &MSXDevice::getName()
{
	if (deviceName != 0) {
		return *deviceName;
	} else {
		string *tmp = new string("No name");
		return *tmp;
	}
}

//protected:
//These are used for save/restoreState see note over
//savefile-structure
bool MSXDevice::writeSaveStateHeader(ofstream &readstream )
{
	return true;
};
bool MSXDevice::checkSaveStateHeader(string &devicestring)
{
	return true;
};
//char* MSXDevice::deviceName;
//char* MSXDevice::deviceVersion;
