// $Id$

#include "XMLElement.hh"
#include "DeviceFactory.hh"
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
#include "MSXRTC.hh"
#include "RomFactory.hh"
#include "MSXRom.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"
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

using std::auto_ptr;
using std::string;

namespace openmsx {

auto_ptr<MSXDevice> DeviceFactory::create(const XMLElement& conf, const EmuTime& time)
{
	const string& type = conf.getName();
	if (type == "PPI") {
		return auto_ptr<MSXDevice>(new MSXPPI(conf, time));
	}
	if (type == "RAM") {
		return auto_ptr<MSXDevice>(new MSXRam(conf, time));
	}
	if (type == "VDP") {
		return auto_ptr<MSXDevice>(new VDP(conf, time));
	}
	if (type == "E6Timer") {
		return auto_ptr<MSXDevice>(new MSXE6Timer(conf, time));
	}
	if (type == "F4Device") {
		return auto_ptr<MSXDevice>(new MSXF4Device(conf, time));
	}
	if (type == "TurboRLeds") {
		return auto_ptr<MSXDevice>(new MSXTurboRLeds(conf, time));
	}
	if (type == "TurboRPause") {
		return auto_ptr<MSXDevice>(new MSXTurboRPause(conf, time));
	}
	if (type == "TurboRPCM") {
		return auto_ptr<MSXDevice>(new MSXTurboRPCM(conf, time));
	}
	if (type == "S1985") {
		return auto_ptr<MSXDevice>(new MSXS1985(conf, time));
	}
	if (type == "S1990") {
		return auto_ptr<MSXDevice>(new MSXS1990(conf, time));
	}
	if (type == "PSG") {
		return auto_ptr<MSXDevice>(new MSXPSG(conf, time));
	}
	if (type == "MSX-MUSIC") {
		return auto_ptr<MSXDevice>(new MSXMusic(conf, time));
	}
	if (type == "FMPAC") {
		return auto_ptr<MSXDevice>(new MSXFmPac(conf, time));
	}
	if (type == "MSX-AUDIO") {
		return auto_ptr<MSXDevice>(new MSXAudio(conf, time));
	}
	if (type == "MusicModuleMIDI") {
		return auto_ptr<MSXDevice>(new MC6850(conf, time));
	}
	if (type == "MoonSound") {
		return auto_ptr<MSXDevice>(new MSXMoonSound(conf, time));
	}
	if (type == "Kanji") {
		return auto_ptr<MSXDevice>(new MSXKanji(conf, time));
	}
	if (type == "Bunsetsu") {
		return auto_ptr<MSXDevice>(new MSXBunsetsu(conf, time));
	}
	if (type == "MemoryMapper") {
		return auto_ptr<MSXDevice>(new MSXMemoryMapper(conf, time));
	}
	if (type == "PanasonicRAM") {
		return auto_ptr<MSXDevice>(new PanasonicRam(conf, time));
	}
	if (type == "RTC") {
		return auto_ptr<MSXDevice>(new MSXRTC(conf, time));
	}
	if (type == "ROM") {
		return RomFactory::create(conf, time);
	}
	if (type == "PrinterPort") {
		return auto_ptr<MSXDevice>(new MSXPrinterPort(conf, time));
	}
	if (type == "SCCplus") { // Note: it's actually called SCC-II
		return auto_ptr<MSXDevice>(new MSXSCCPlusCart(conf, time));
	}
	if (type == "WD2793") {
		return auto_ptr<MSXDevice>(new PhilipsFDC(conf, time));
	}
	if (type == "Microsol") {
		return auto_ptr<MSXDevice>(new MicrosolFDC(conf, time));
	}
	if (type == "MB8877A") {
		return auto_ptr<MSXDevice>(new NationalFDC(conf, time));
	}
	if (type == "TC8566AF") {
		return auto_ptr<MSXDevice>(new TurboRFDC(conf, time));
	}
	if (type == "SunriseIDE") {
		return auto_ptr<MSXDevice>(new SunriseIDE(conf, time));
	}
	if (type == "Matsushita") {
		return auto_ptr<MSXDevice>(new MSXMatsushita(conf, time));
	}
	if (type == "Kanji12") {
		return auto_ptr<MSXDevice>(new MSXKanji12(conf, time));
	}
	if (type == "MSX-MIDI") {
		return auto_ptr<MSXDevice>(new MSXMidi(conf, time));
	}
	if (type == "MSX-RS232") {
		return auto_ptr<MSXDevice>(new MSXRS232(conf, time));
	}
	if (type == "MegaRam") {
		return auto_ptr<MSXDevice>(new MSXMegaRam(conf, time));
	}
	if (type == "PAC") {
		return auto_ptr<MSXDevice>(new MSXPac(conf, time));
	}
	if (type == "HBI55") {
		return auto_ptr<MSXDevice>(new MSXHBI55(conf, time));
	}
	if (type == "DebugDevice") {
		return auto_ptr<MSXDevice>(new DebugDevice(conf, time));
	}
	if (type == "V9990") {
		return auto_ptr<MSXDevice>(new V9990(conf, time));
	}

	throw FatalError("Unknown device \"" + type + "\" specified in configuration");
}

} // namespace openmsx
