// $Id$

#include "config.h"

#include "MSXConfig.hh"
#include "DeviceFactory.hh"
#include "MSXRam.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXF4Device.hh"
#include "MSXTurboRLeds.hh"
#include "MSXTurboRPause.hh"
#include "MSXS1985.hh"
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
#include "FDCFactory.hh"
#include "SunriseIDE.hh"
#include "MSXMatsushita.hh"


MSXDevice *DeviceFactory::create(MSXConfig::Device *conf, const EmuTime &time)
{
	const std::string type = conf->getType();
	if (type == "CPU") {
		return MSXCPU::instance();
	} 
	if (type == "MapperIO") {
		return MSXMapperIO::instance();
	} 
	if (type == "PPI") {
		return new MSXPPI(conf, time);
	} 
	if (type == "RAM") {
		return new MSXRam(conf, time);
	} 
	if (type == "VDP") {
		return new VDP(conf, time);
	} 
	if (type == "E6Timer") {
		return new MSXE6Timer(conf, time);
	} 
	if (type == "F4Device") {
		return new MSXF4Device(conf, time);
	} 
	if (type == "TurboRLeds") {
		return new MSXTurboRLeds(conf, time);
	} 
	if (type == "TurboRPause") {
		return new MSXTurboRPause(conf, time);
	} 
	if (type == "S1985") {
		return new MSXS1985(conf, time);
	} 
	if (type == "S1990") {
		return new MSXS1990(conf, time);
	} 
	if (type == "PSG") {
		return new MSXPSG(conf, time);
	} 
	if (type == "Music") {
		return new MSXMusic(conf, time);
	} 
	if (type == "FM-PAC") {
		return new MSXFmPac(conf, time);
	} 
	if (type == "Audio") {
		return new MSXAudio(conf, time);
	} 
	if (type == "Audio-Midi") {
		return new MC6850(conf, time);
	} 
	if (type == "Kanji") {
		return new MSXKanji(conf, time);
	} 
	if (type == "MemoryMapper") {
		return new MSXMemoryMapper(conf, time);
	} 
	if (type == "RTC") {
		return new MSXRTC(conf, time);
	} 
	if (type == "Rom") {
		return new MSXRom(conf, time);
	} 
	if (type == "PrinterPort") {
		return new MSXPrinterPort(conf, time);
	} 
	if (type == "SCCPlusCart") {
		return new MSXSCCPlusCart(conf, time);
	} 
	if (type == "FDC") {
		return FDCFactory::create(conf, time);
	}
	if (type == "SunriseIDE") {
		return new SunriseIDE(conf, time);
	}
	if (type == "Matsushita") {
		return new MSXMatsushita(conf, time);
	}
	PRT_ERROR("Unknown device specified in configuration");
	return NULL;
}

