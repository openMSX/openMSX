// $Id$

#include "config.h"

#include "MSXConfig.hh"
#include "DeviceFactory.hh"
#include "MSXCPUInterface.hh"
#include "MSXRam.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXF4Device.hh"
#include "MSXTurboRLeds.hh"
#include "MSXTurboRPause.hh"
#include "MSXTurboRPCM.hh"
#include "MSXS1985.hh"
#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "MSXMapperIO.hh"
#include "MSXPSG.hh"
#include "MSXMusic.hh"
#include "MSXMusic_2.hh"
#include "MSXFmPac.hh"
#include "MSXAudio.hh"
#include "MSXMoonSound.hh"
#include "MC6850.hh"
#include "MSXKanji.hh"
#include "MSXBunsetsu.hh"
#include "MSXMemoryMapper.hh"
#include "PanasonicRam.hh"
#include "PanasonicRom.hh"
#include "MSXRTC.hh"
#include "RomFactory.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "FDCFactory.hh"
#include "SunriseIDE.hh"
#include "MSXMatsushita.hh"
#include "MSXKanji12.hh"
#include "MSXMidi.hh"
#include "MSXMegaRam.hh"
#include "MSXPac.hh"
#include "MSXHBI55.hh"
#include "DebugDevice.hh"


// TODO: Add the switched device to the config files.
static void createDeviceSwitch() {
	static bool deviceSwitchCreated = false;
	if (!deviceSwitchCreated) {
		MSXDeviceSwitch *deviceSwitch = MSXDeviceSwitch::instance();
		for (byte port = 0x40; port < 0x50; port++) {
			MSXCPUInterface::instance()->register_IO_Out(port, deviceSwitch);
			MSXCPUInterface::instance()->register_IO_In(port, deviceSwitch);
		}
		deviceSwitchCreated = true;
	}
}

MSXDevice *DeviceFactory::create(Device *conf, const EmuTime &time)
{
	const std::string type = conf->getType();
	MSXCPUInterface *cpuInterface = MSXCPUInterface::instance();
	if (type == "CPU") {
		return MSXCPU::instance();
	}
	if (type == "MapperIO") {
		MSXMapperIO *mapperIO = MSXMapperIO::instance();
		cpuInterface->register_IO_Out(0xFC, mapperIO);
		cpuInterface->register_IO_Out(0xFD, mapperIO);
		cpuInterface->register_IO_Out(0xFE, mapperIO);
		cpuInterface->register_IO_Out(0xFF, mapperIO);
		cpuInterface->register_IO_In(0xFC, mapperIO);
		cpuInterface->register_IO_In(0xFD, mapperIO);
		cpuInterface->register_IO_In(0xFE, mapperIO);
		cpuInterface->register_IO_In(0xFF, mapperIO);
		return mapperIO;
	}
	if (type == "PPI") {
		MSXPPI *ppi = new MSXPPI(conf, time);
		cpuInterface->register_IO_Out(0xA8, ppi);
		cpuInterface->register_IO_Out(0xA9, ppi);
		cpuInterface->register_IO_Out(0xAA, ppi);
		cpuInterface->register_IO_Out(0xAB, ppi);
		cpuInterface->register_IO_In(0xA8, ppi);
		cpuInterface->register_IO_In(0xA9, ppi);
		cpuInterface->register_IO_In(0xAA, ppi);
		cpuInterface->register_IO_In(0xAB, ppi);
		return ppi;
	}
	if (type == "RAM") {
		return new MSXRam(conf, time);
	}
	if (type == "VDP") {
		VDP *vdp = new VDP(conf, time);
		cpuInterface->register_IO_Out(0x98, vdp);
		cpuInterface->register_IO_Out(0x99, vdp);
		if (!vdp->isMSX1VDP()) {
			cpuInterface->register_IO_Out(0x9A, vdp);
			cpuInterface->register_IO_Out(0x9B, vdp);
		}
		cpuInterface->register_IO_In(0x98, vdp);
		cpuInterface->register_IO_In(0x99, vdp);
		return vdp;
	}
	if (type == "E6Timer") {
		MSXE6Timer *timer = new MSXE6Timer(conf, time);
		cpuInterface->register_IO_Out(0xE6, timer);
		cpuInterface->register_IO_In(0xE6, timer);
		cpuInterface->register_IO_In(0xE7, timer);
		return timer;
	}
	if (type == "F4Device") {
		MSXF4Device *f4Device = new MSXF4Device(conf, time);
		cpuInterface->register_IO_Out(0xF4, f4Device);
		cpuInterface->register_IO_In(0xF4, f4Device);
		return f4Device;
	}
	if (type == "TurboRLeds") {
		MSXTurboRLeds *leds = new MSXTurboRLeds(conf, time);
		cpuInterface->register_IO_Out(0xA7, leds);
		return leds;
	}
	if (type == "TurboRPause") {
		return new MSXTurboRPause(conf, time);
	}
	if (type == "TurboRPCM") {
		MSXTurboRPCM *pcm = new MSXTurboRPCM(conf, time);
		cpuInterface->register_IO_Out(0xA4, pcm);
		cpuInterface->register_IO_Out(0xA5, pcm);
		cpuInterface->register_IO_In(0xA4, pcm);
		cpuInterface->register_IO_In(0xA5, pcm);
		return pcm;
	}
	if (type == "S1985") {
		createDeviceSwitch();
		return new MSXS1985(conf, time);
	}
	if (type == "S1990") {
		MSXS1990 *engine = new MSXS1990(conf, time);
		cpuInterface->register_IO_Out(0xE4, engine);
		cpuInterface->register_IO_Out(0xE5, engine);
		cpuInterface->register_IO_In(0xE4, engine);
		cpuInterface->register_IO_In(0xE5, engine);
		return engine;
	}
	if (type == "PSG") {
		MSXPSG *psg = new MSXPSG(conf, time);
		cpuInterface->register_IO_Out(0xA0, psg);
		cpuInterface->register_IO_Out(0xA1, psg);
		cpuInterface->register_IO_In(0xA2, psg);
		return psg;
	}
	if (type == "Music") {
		MSXMusic *music = new MSXMusic(conf, time);
		cpuInterface->register_IO_Out(0x7C, music);
		cpuInterface->register_IO_Out(0x7D, music);
		return music;
	}
	if (type == "Music_2") {
		MSXMusic_2 *music = new MSXMusic_2(conf, time);
		cpuInterface->register_IO_Out(0x7C, music);
		cpuInterface->register_IO_Out(0x7D, music);
		return music;
	}
	if (type == "FM-PAC") {
		MSXFmPac *fmpac = new MSXFmPac(conf, time);
		cpuInterface->register_IO_Out(0x7C, fmpac);
		cpuInterface->register_IO_Out(0x7D, fmpac);
		return fmpac;
	}
	if (type == "Audio") {
		MSXAudio *audio = new MSXAudio(conf, time);
		byte base = conf->hasParameter("number")
			&& conf->getParameter("number") == "2"
			? 0xC2 : 0xC0;
		cpuInterface->register_IO_Out(base + 0, audio);
		cpuInterface->register_IO_Out(base + 1, audio);
		cpuInterface->register_IO_In(base + 0, audio);
		cpuInterface->register_IO_In(base + 1, audio);
		return audio;
	}
	if (type == "Audio-Midi") {
		MC6850 *midi = new MC6850(conf, time);
		MSXCPUInterface::instance()->register_IO_Out(0x00, midi);
		MSXCPUInterface::instance()->register_IO_Out(0x01, midi);
		MSXCPUInterface::instance()->register_IO_In(0x04, midi);
		MSXCPUInterface::instance()->register_IO_In(0x05, midi);
		return midi;
	}
	if (type == "MoonSound") {
		MSXMoonSound *moonsound = new MSXMoonSound(conf, time);
		cpuInterface->register_IO_In(0x7E, moonsound);
		cpuInterface->register_IO_In(0x7F, moonsound);
		cpuInterface->register_IO_Out(0x7E, moonsound);
		cpuInterface->register_IO_Out(0x7F, moonsound);
		cpuInterface->register_IO_In(0xC4, moonsound);
		cpuInterface->register_IO_In(0xC5, moonsound);
		cpuInterface->register_IO_In(0xC6, moonsound);
		cpuInterface->register_IO_In(0xC7, moonsound);
		cpuInterface->register_IO_Out(0xC4, moonsound);
		cpuInterface->register_IO_Out(0xC5, moonsound);
		cpuInterface->register_IO_Out(0xC6, moonsound);
		cpuInterface->register_IO_Out(0xC7, moonsound);
		return moonsound;
	}
	if (type == "Kanji") {
		MSXKanji *kanji = new MSXKanji(conf, time);
		cpuInterface->register_IO_Out(0xD8, kanji);
		cpuInterface->register_IO_Out(0xD9, kanji);
		cpuInterface->register_IO_In(0xD9, kanji);
		// TODO: In the machine description, port mapping should only assign
		//       these on a 256K Kanji ROM.
		cpuInterface->register_IO_Out(0xDA, kanji);
		cpuInterface->register_IO_Out(0xDB, kanji);
		cpuInterface->register_IO_In(0xDB, kanji);
		return kanji;
	}
	if (type == "Bunsetsu") {
		return new MSXBunsetsu(conf, time);
	}
	if (type == "MemoryMapper") {
		return new MSXMemoryMapper(conf, time);
	}
	if (type == "PanasonicRam") {
		return new PanasonicRam(conf, time);
	}
	if (type == "PanasonicRom") {
		return new PanasonicRom(conf, time);
	}
	if (type == "RTC") {
		MSXRTC *rtc = new MSXRTC(conf, time);
		cpuInterface->register_IO_Out(0xB4, rtc);
		cpuInterface->register_IO_Out(0xB5, rtc);
		cpuInterface->register_IO_In(0xB5, rtc);
		return rtc;
	}
	if (type == "Rom") {
		return RomFactory::create(conf, time);
	}
	if (type == "PrinterPort") {
		MSXPrinterPort *printerPort = new MSXPrinterPort(conf, time);
		cpuInterface->register_IO_Out(0x90, printerPort);
		cpuInterface->register_IO_Out(0x91, printerPort);
		cpuInterface->register_IO_In(0x90, printerPort);
		return printerPort;
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
		createDeviceSwitch();
		return new MSXMatsushita(conf, time);
	}
	if (type == "Kanji12") {
		createDeviceSwitch();
		return new MSXKanji12(conf, time);
	}
	if (type == "MSX-Midi") {
		MSXMidi *msxmidi = new MSXMidi(conf, time);
		for (byte port = 0xE8; port <= 0xEF; port++) {
			cpuInterface->register_IO_In (port, msxmidi);
			cpuInterface->register_IO_Out(port, msxmidi);
		}
		return msxmidi;
	}
	if (type == "MegaRam") {
		MSXMegaRam *megaram = new MSXMegaRam(conf, time);
		cpuInterface->register_IO_In (0x8E, megaram);
		cpuInterface->register_IO_Out(0x8E, megaram);
		return megaram;
	}
	if (type == "PAC") {
		return new MSXPac(conf, time);
	}
	if (type == "HBI55") {
		MSXHBI55 *hbi55 = new MSXHBI55(conf, time);
		cpuInterface->register_IO_Out(0xB0, hbi55);
		cpuInterface->register_IO_Out(0xB1, hbi55);
		cpuInterface->register_IO_Out(0xB2, hbi55);
		cpuInterface->register_IO_Out(0xB3, hbi55);
		cpuInterface->register_IO_In (0xB0, hbi55);
		cpuInterface->register_IO_In (0xB1, hbi55);
		cpuInterface->register_IO_In (0xB2, hbi55);
		cpuInterface->register_IO_In (0xB3, hbi55);
		return hbi55;
	}
	
	if (type == "DebugDevice") {
		DebugDevice * debugDevice = new DebugDevice(conf, time);
		cpuInterface->register_IO_Out(0x2E, debugDevice);
		cpuInterface->register_IO_Out(0x2F, debugDevice);
		return debugDevice;
	}
	PRT_ERROR("Unknown device \""<<type<<"\" specified in configuration");
	return NULL;
}
