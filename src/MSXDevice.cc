// $Id$
// 
// #include "MSXDevice.hh
#include "iostream.h"
//using namespace std;

MSXDevice::MSXDevice(void)
{
//TODO: something usefull here ??
	cout << "instantiating an MSXDevice object\n";
};

MSXDevice::~MSXDevice(void)
{
//TODO: something usefull here ??
	cout << "destructing an MSXDevice object";
};
MSXDevice* MSXDevice::instantiate(void) 
{
	cout << "Something is wrong here\nNobody should be calling MSXDevice::instantiate()";
	return new MSXDevice();
};

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
	return;
}
void MSXDevice::start()
{
	return;
}
void MSXDevice::stop()
{
	return;
}
void MSXDevice::reset()
{
	return;
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
