// $Id$

#include "msxconfig.hh"
#include "devicefactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXCPU.hh"
#include "MSXPSG.hh"
#include "MSXMusic.hh"
#include "MSXFmPac.hh"
#include "MSXAudio.hh"
#include "MSXKanji.hh"
#include "MSXMemoryMapper.hh"
#include "MSXRTC.hh"
#include "MSXGameCartridge.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
//#include "MSXPostLoad.hh"


MSXDevice *deviceFactory::create(MSXConfig::Device *conf, const EmuTime &time) {
	MSXDevice *device = NULL;
	if (conf->getType()=="CPU") {
		device = MSXCPU::instance();
	} else
	if (conf->getType()=="PPI") {
		device = new MSXPPI(conf, time);
	} else
	if (conf->getType()=="Rom16KB") {
		device = new MSXRom16KB(conf, time);
	} else
	if (conf->getType()=="Simple64KB") {
		device = new MSXSimple64KB(conf, time);
	} else
	if (conf->getType()=="VDP") {
		device = new VDP(conf, time);
	} else
	if (conf->getType()=="E6Timer") {
		device = new MSXE6Timer(conf, time);
	} else
	if (conf->getType()=="PSG") {
		device = new MSXPSG(conf, time);
	} else
	if (conf->getType()=="Music") {
		device = new MSXMusic(conf, time);
	} else
	if (conf->getType()=="FM-PAC") {
		device = new MSXFmPac(conf, time);
	} else
	if (conf->getType()=="Audio") {
		device = new MSXAudio(conf, time);
	} else
	if (conf->getType()=="Kanji") {
		device = new MSXKanji(conf, time);
	} else
	if (conf->getType()=="MemoryMapper") {
		device = new MSXMemoryMapper(conf, time);
	} else
	if (conf->getType()=="RTC") {
		device = new MSXRTC(conf, time);
	} else
	if (conf->getType()=="GameCartridge") {
		device = new MSXGameCartridge(conf, time);
	} else
	if (conf->getType()=="PrinterPort") {
		device = new MSXPrinterPort(conf, time);
	} else
	if (conf->getType()=="SCCPlusCart") {
		device = new MSXSCCPlusCart(conf, time);
	}
//	} else
//	if (conf->getType()=="PostLoad") {
//		device = new MSXPostLoad(conf, time);
//	}
	if (device == NULL)
		PRT_ERROR("Unknown device specified in configuration");
	return device;
}

