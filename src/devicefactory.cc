// $Id$

#include "msxconfig.hh"
#include "devicefactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "MSXPPI.hh"
#include "MSXTMS9928a.hh"
#include "MSXE6Timer.hh"
#include "MSXCPU.hh"
#include "MSXPSG.hh"
#include "MSXKanji.hh"
#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"


MSXDevice *deviceFactory::create(MSXConfig::Device *conf) {
	MSXDevice *device = NULL;
	if (conf->getType()=="MotherBoard") {
		device = MSXMotherBoard::instance();
	} else
	if (conf->getType()=="Rom16KB") {
		device = new MSXRom16KB();
	} else
	if (conf->getType()=="Simple64KB") {
		device = new MSXSimple64KB();
	} else
	if (conf->getType()=="PPI") {
		device = MSXPPI::instance();
	} else
	if (conf->getType()=="TMS9928a") {
		device = new MSXTMS9928a();
	} else
	if (conf->getType()=="E6Timer") {
		device = new MSXE6Timer();
	} else
	if (conf->getType()=="CPU") {
		device = MSXCPU::instance();
	} else
	if (conf->getType()=="PSG") {
		device = new MSXPSG();
	} else
	if (conf->getType()=="Kanji") {
		device = new MSXKanji();
	} else
	if (conf->getType()=="MemoryMapper") {
		device = new MSXMemoryMapper();
	} else
	if (conf->getType()=="MapperIO") {
		device = MSXMapperIO::instance();
	}

	if (device == NULL)
		PRT_ERROR("Unknown device specified in configuration");
	device->setConfigDevice(conf);
	return device;
}

