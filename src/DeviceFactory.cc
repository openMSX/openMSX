// $Id$

#include "config.h"

#include "MSXConfig.hh"
#include "DeviceFactory.hh"
#include "MSXSimple64KB.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXF4Device.hh"
#include "MSXTurboRLeds.hh"
#include "MSXTurboRPause.hh"
#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "MSXMapperIO.hh"
#include "MSXPSG.hh"
#include "MSXMusic.hh"
#include "MSXFmPac.hh"
#include "MSXAudio.hh"
#include "MC6850.hh"
#include "MSXKanji.hh"
#include "MSXMemoryMapper.hh"
#include "MSXRTC.hh"
#include "MSXRom.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "MSXARCdebug.hh"
#include "MSXFDC.hh"
#include "SunriseIDE.hh"


MSXDevice *DeviceFactory::create(MSXConfig::Device *conf, const EmuTime &time) {
	MSXDevice *device = NULL;
	if (conf->getType()=="CPU") {
		device = MSXCPU::instance();
	} else
	if (conf->getType()=="MapperIO") {
		device = MSXMapperIO::instance();
	} else
	if (conf->getType()=="PPI") {
		device = new MSXPPI(conf, time);
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
	if (conf->getType()=="F4Device") {
		device = new MSXF4Device(conf, time);
	} else
	if (conf->getType()=="TurboRLeds") {
		device = new MSXTurboRLeds(conf, time);
	} else
	if (conf->getType()=="TurboRPause") {
		device = new MSXTurboRPause(conf, time);
	} else
	if (conf->getType()=="S1990") {
		device = new MSXS1990(conf, time);
	} else
	if (conf->getType()=="PSG") {
		device = new MSXPSG(conf, time);
	} else
#ifndef DONT_WANT_MSXMUSIC
	if (conf->getType()=="Music") {
		device = new MSXMusic(conf, time);
	} else
#endif
#ifndef DONT_WANT_FMPAC
	if (conf->getType()=="FM-PAC") {
		device = new MSXFmPac(conf, time);
	} else
#endif
	if (conf->getType()=="Audio") {
		device = new MSXAudio(conf, time);
	} else
	if (conf->getType()=="Audio-Midi") {
		device = new MC6850(conf, time);
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
	if (conf->getType()=="Rom") {
		device = new MSXRom(conf, time);
	} else
	if (conf->getType()=="PrinterPort") {
		device = new MSXPrinterPort(conf, time);
	} else
#ifndef DONT_WANT_SCC
	if (conf->getType()=="SCCPlusCart") {
		device = new MSXSCCPlusCart(conf, time);
	} else
#endif
	if (conf->getType()=="ARCdebug") {
		device = new MSXARCdebug(conf, time);
	} else 
	if (conf->getType()=="FDC") {
		device = new MSXFDC(conf, time);
	}
	if (conf->getType()=="SunriseIDE") {
		device = new SunriseIDE(conf, time);
	}
	if (device == NULL)
		PRT_ERROR("Unknown device specified in configuration");
	return device;
}

