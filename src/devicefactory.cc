// $Id$

#include "msxconfig.hh"
#include "devicefactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "MSXPPI.hh"

//MSXDevice *deviceFactory::create(const string &type){
MSXDevice *deviceFactory::create(MSXConfig::Device *conf){
	MSXDevice *device=0;
	if ( conf->getType().compare("MotherBoard") == 0 ){
		// if 0 then strings are equal
		MSXMotherBoard::instance()->setConfigDevice(conf);
	};

	if ( conf->getType().compare("Rom16KB") == 0 ){
		// if 0 then strings are equal
		device=new MSXRom16KB();
	};

	if ( conf->getType().compare("Simple64KB") == 0 ){ 
		// if 0 then strings are equal
		device=new MSXSimple64KB();
	};

	if ( conf->getType().compare("PPI") == 0 ){ 
		// if 0 then strings are equal
		device=new MSXPPI();
	};

	if (device != 0){
		device->setConfigDevice(conf);
	};
	return device;
}

