// $Id$

#include "msxconfig.hh"
#include "devicefactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "MSXPPI.hh"
#include "MSXTMS9928a.hh"
#include "MSXE6Timer.hh"

MSXDevice *deviceFactory::create(MSXConfig::Device *conf) {
	MSXDevice *device = 0;
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

	if ( conf->getType().compare("TMS9928a") == 0 ){ 
		// if 0 then strings are equal
		device=new MSXTMS9928a();
	};

	if ( conf->getType().compare("MSXE6Timer") == 0 ){ 
		// if 0 then strings are equal
		device=new MSXE6Timer();
	};

	//assert (device != 0);
	if (device == 0) {
		PRT_DEBUG("device == 0  in devicefactory"); // TODO check
	} else {
		device->setConfigDevice(conf);
	}
	return device;
}

