// $Id$

#include "Config.hh"
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
#include "MSXPSG.hh"
#include "MSXMusic.hh"
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
#include "MSXRom.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "FDCFactory.hh"
#include "SunriseIDE.hh"
#include "MSXMatsushita.hh"
#include "MSXKanji12.hh"
#include "MSXMidi.hh"
#include "MSXRS232.hh"
#include "MSXMegaRam.hh"
#include "MSXPac.hh"
#include "MSXHBI55.hh"
#include "DebugDevice.hh"
#include "V9990.hh"

namespace openmsx {

// TODO: Add the switched device to the config files.
static void createDeviceSwitch()
{
	static bool deviceSwitchCreated = false;
	if (!deviceSwitchCreated) {
		MSXDeviceSwitch *deviceSwitch = &MSXDeviceSwitch::instance();
		for (byte port = 0x40; port < 0x50; port++) {
			MSXCPUInterface::instance().register_IO_Out(port, deviceSwitch);
			MSXCPUInterface::instance().register_IO_In(port, deviceSwitch);
		}
		deviceSwitchCreated = true;
	}
}

MSXDevice *DeviceFactory::create(Config* conf, const EmuTime& time)
{
	const string type = conf->getType();
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
	if (type == "TurboRPCM") {
		return new MSXTurboRPCM(conf, time);
	}
	if (type == "S1985") {
		createDeviceSwitch();
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
	if (type == "FMPAC") {
		return new MSXFmPac(conf, time);
	}
	if (type == "Audio") {
		return new MSXAudio(conf, time);
	}
	if (type == "Audio-Midi") {
		return new MC6850(conf, time);
	}
	if (type == "MoonSound") {
		return new MSXMoonSound(conf, time);
	}
	if (type == "Kanji") {
		return new MSXKanji(conf, time);
	}
	if (type == "Bunsetsu") {
		return new MSXBunsetsu(conf, time);
	}
	if (type == "MemoryMapper") {
		return new MSXMemoryMapper(conf, time);
	}
	if (type == "PanasonicRAM") {
		return new PanasonicRam(conf, time);
	}
	if (type == "PanasonicROM") {
		return new PanasonicRom(conf, time);
	}
	if (type == "RTC") {
		return new MSXRTC(conf, time);
	}
	if (type == "ROM") {
		return RomFactory::create(conf, time);
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
		createDeviceSwitch();
		return new MSXMatsushita(conf, time);
	}
	if (type == "Kanji12") {
		createDeviceSwitch();
		return new MSXKanji12(conf, time);
	}
	if (type == "MSX-Midi") {
		return new MSXMidi(conf, time);
	}
	if (type == "MSX-RS232") {
		return new MSXRS232(conf, time);
	}
	if (type == "MegaRam") {
		return new MSXMegaRam(conf, time);
	}
	if (type == "PAC") {
		return new MSXPac(conf, time);
	}
	if (type == "HBI55") {
		return new MSXHBI55(conf, time);
	}
	if (type == "DebugDevice") {
		return new DebugDevice(conf, time);
	}
	if (type == "V9990") {
		return new V9990(conf, time);
	}

	throw FatalError("Unknown device \"" + type + "\" specified in configuration");
}

} // namespace openmsx
