#include "MSXDevice.hh"
#include "iostream.h"
//using namespace std;

MSXDevice::MSXDevice(void)
{
//TODO: something usefull here ??
	cout << "instantiating an MSXDevice object";
}

MSXDevice::~MSXDevice(void)
{
//TODO: something usefull here ??
	cout << "destructing an MSXDevice object";
};
MSXDevice* MSXDevice::instantiate(void) 
{
	cout << "Something is wrong here\nNobody should be calling MSXDevice::instantiate()";
	return new MSXDevice;
};

		
// interaction with CPU
byte MSXDevice::readMem(word address,UINT64 TStates)
{
	return (byte)255;
}
void MSXDevice::writeMem(word address,byte value,UINT64 TStates)
{
	return;
}
byte MSXDevice:: readIO(byte port,UINT64 TStates)
{
	return (byte)255;
}
void MSXDevice::writeIO(byte port,byte value,UINT64 TStates)
{
	return;
}
void MSXDevice::executeUntilEmuTime(UINT64 TStates)
{
    return;
}

int MSXDevice::executeTStates(int TStates)
{
	return (int)0;
}

int MSXDevice::getUsedTStates(void)
{
	return (int)0;
}


//
void MSXDevice::init()
{
}
void MSXDevice::reset()
{
}
//
void MSXDevice::saveState(ofstream writestream)
{
}
void MSXDevice::restoreState(char *devicestring,ifstream readstream)
{
}
void MSXDevice::setParameter(char *param,char *valuelist)
{
}
char* MSXDevice::getParameter(char *param)
{
}
int MSXDevice::getNrParameters()
{
}
char* MSXDevice::getParameterTxt(int nr)
{
}
char* MSXDevice::getParamShortHelp(int nr)
{
}
char* MSXDevice::getParamLongHelp(int nr)
{
}

//protected:
//These are used for save/restoreState see note over
//savefile-structure
bool MSXDevice::writeSaveStateHeader(ofstream readstream )
{
};
bool MSXDevice::checkSaveStateHeader(char *devicestring)
{
};
//char* MSXDevice::deviceName;
//char* MSXDevice::deviceVersion;
