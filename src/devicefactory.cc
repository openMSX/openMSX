// $Id$

#include "msxconfig.hh"
#include "devicefactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "MSXPPI.hh"
#include "MSXCassettePort.hh"
#include "MSXTMS9928a.hh"
#include "MSXE6Timer.hh"
#include "MSXCPU.hh"
#include "MSXPSG.hh"
#include "MSXMusic.hh"
#include "MSXKanji.hh"
#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXRTC.hh"
#include "MSXRealTime.hh"
#include "MSXMegaRom.hh"
#include "MSXKonamiSynthesizer.hh"
#include "MSXPrinterPort.hh"
//#include "MSXPostLoad.hh"


MSXDevice *deviceFactory::create(MSXConfig::Device *conf) {
	MSXDevice *device = NULL;
	if (conf->getType()=="MotherBoard") {
		device = new MSXMotherBoard(conf);
	} else
	if (conf->getType()=="Rom16KB") {
		device = new MSXRom16KB(conf);
	} else
	if (conf->getType()=="Simple64KB") {
		device = new MSXSimple64KB(conf);
	} else
	if (conf->getType()=="PPI") {
		device = new MSXPPI(conf);
	} else
	if (conf->getType()=="CassettePort") {
		device = new MSXCassettePort(conf);
	} else
	if (conf->getType()=="TMS9928a") {
		device = new MSXTMS9928a(conf);
	} else
	if (conf->getType()=="E6Timer") {
		device = new MSXE6Timer(conf);
	} else
	if (conf->getType()=="CPU") {
		device = new MSXCPU(conf);
	} else
	if (conf->getType()=="PSG") {
		device = new MSXPSG(conf);
	} else
	if (conf->getType()=="Music") {
		device = new MSXMusic(conf);
	} else
	if (conf->getType()=="Kanji") {
		device = new MSXKanji(conf);
	} else
	if (conf->getType()=="MemoryMapper") {
		device = new MSXMemoryMapper(conf);
	} else
	if (conf->getType()=="MapperIO") {
		device = new MSXMapperIO(conf);
	} else
	if (conf->getType()=="RTC") {
		device = new MSXRTC(conf);
	} else
	if (conf->getType()=="RealTime") {
		device = new MSXRealTime(conf);
	} else
	if (conf->getType()=="MegaRom") {
		device = new MSXMegaRom(conf);
	} else
	if (conf->getType()=="PrinterPort") {
		device = new MSXPrinterPort(conf);
	} else
	if (conf->getType()=="KonamiSynthesizer") {
		device = new MSXKonamiSynthesizer(conf);
	}
//	} else
//	if (conf->getType()=="PostLoad") {
//		device = new MSXPostLoad(conf);
//	}
	if (device == NULL)
		PRT_ERROR("Unknown device specified in configuration");
	return device;
}

