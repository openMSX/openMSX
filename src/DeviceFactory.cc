// $Id$

#include "DeviceFactory.hh"
#include "XMLElement.hh"
#include "MSXRam.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXF4Device.hh"
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
#include "ADVram.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {

auto_ptr<MSXDevice> DeviceFactory::create(
	MSXMotherBoard& motherBoard, const XMLElement& conf, const EmuTime& time)
{
	const string& type = conf.getName();
	if (type == "PPI") {
		return auto_ptr<MSXDevice>(new MSXPPI(motherBoard, conf, time));
	}
	if (type == "RAM") {
		return auto_ptr<MSXDevice>(new MSXRam(motherBoard, conf, time));
	}
	if (type == "VDP") {
		return auto_ptr<MSXDevice>(new VDP(motherBoard, conf, time));
	}
	if (type == "E6Timer") {
		return auto_ptr<MSXDevice>(new MSXE6Timer(motherBoard, conf, time));
	}
	if (type == "F4Device") {
		return auto_ptr<MSXDevice>(new MSXF4Device(motherBoard, conf, time));
	}
	if (type == "TurboRLeds") {
		// deprecated, remove in next version
		return auto_ptr<MSXDevice>(NULL);
	}
	if (type == "TurboRPause") {
		return auto_ptr<MSXDevice>(new MSXTurboRPause(motherBoard, conf, time));
	}
	if (type == "TurboRPCM") {
		return auto_ptr<MSXDevice>(new MSXTurboRPCM(motherBoard, conf, time));
	}
	if (type == "S1985") {
		return auto_ptr<MSXDevice>(new MSXS1985(motherBoard, conf, time));
	}
	if (type == "S1990") {
		return auto_ptr<MSXDevice>(new MSXS1990(motherBoard, conf, time));
	}
	if (type == "PSG") {
		return auto_ptr<MSXDevice>(new MSXPSG(motherBoard, conf, time));
	}
	if (type == "MSX-MUSIC") {
		return auto_ptr<MSXDevice>(new MSXMusic(motherBoard, conf, time));
	}
	if (type == "FMPAC") {
		return auto_ptr<MSXDevice>(new MSXFmPac(motherBoard, conf, time));
	}
	if (type == "MSX-AUDIO") {
		return auto_ptr<MSXDevice>(new MSXAudio(motherBoard, conf, time));
	}
	if (type == "MusicModuleMIDI") {
		return auto_ptr<MSXDevice>(new MC6850(motherBoard, conf, time));
	}
	if (type == "MoonSound") {
		return auto_ptr<MSXDevice>(new MSXMoonSound(motherBoard, conf, time));
	}
	if (type == "Kanji") {
		return auto_ptr<MSXDevice>(new MSXKanji(motherBoard, conf, time));
	}
	if (type == "Bunsetsu") {
		return auto_ptr<MSXDevice>(new MSXBunsetsu(motherBoard, conf, time));
	}
	if (type == "MemoryMapper") {
		return auto_ptr<MSXDevice>(new MSXMemoryMapper(motherBoard, conf, time));
	}
	if (type == "PanasonicRAM") {
		return auto_ptr<MSXDevice>(new PanasonicRam(motherBoard, conf, time));
	}
	if (type == "RTC") {
		return auto_ptr<MSXDevice>(new MSXRTC(motherBoard, conf, time));
	}
	if (type == "ROM") {
		return RomFactory::create(motherBoard, conf, time);
	}
	if (type == "PrinterPort") {
		return auto_ptr<MSXDevice>(new MSXPrinterPort(motherBoard, conf, time));
	}
	if (type == "SCCplus") { // Note: it's actually called SCC-I
		return auto_ptr<MSXDevice>(new MSXSCCPlusCart(motherBoard, conf, time));
	}
	if (type == "WD2793") {
		return auto_ptr<MSXDevice>(new PhilipsFDC(motherBoard, conf, time));
	}
	if (type == "Microsol") {
		return auto_ptr<MSXDevice>(new MicrosolFDC(motherBoard, conf, time));
	}
	if (type == "MB8877A") {
		return auto_ptr<MSXDevice>(new NationalFDC(motherBoard, conf, time));
	}
	if (type == "TC8566AF") {
		return auto_ptr<MSXDevice>(new TurboRFDC(motherBoard, conf, time));
	}
	if (type == "SunriseIDE") {
		return auto_ptr<MSXDevice>(new SunriseIDE(motherBoard, conf, time));
	}
	if (type == "Matsushita") {
		return auto_ptr<MSXDevice>(new MSXMatsushita(motherBoard, conf, time));
	}
	if (type == "Kanji12") {
		return auto_ptr<MSXDevice>(new MSXKanji12(motherBoard, conf, time));
	}
	if (type == "MSX-MIDI") {
		return auto_ptr<MSXDevice>(new MSXMidi(motherBoard, conf, time));
	}
	if (type == "MSX-RS232") {
		return auto_ptr<MSXDevice>(new MSXRS232(motherBoard, conf, time));
	}
	if (type == "MegaRam") {
		return auto_ptr<MSXDevice>(new MSXMegaRam(motherBoard, conf, time));
	}
	if (type == "PAC") {
		return auto_ptr<MSXDevice>(new MSXPac(motherBoard, conf, time));
	}
	if (type == "HBI55") {
		return auto_ptr<MSXDevice>(new MSXHBI55(motherBoard, conf, time));
	}
	if (type == "DebugDevice") {
		return auto_ptr<MSXDevice>(new DebugDevice(motherBoard, conf, time));
	}
	if (type == "V9990") {
		return auto_ptr<MSXDevice>(new V9990(motherBoard, conf, time));
	}
	if (type == "ADVram") {
		return auto_ptr<MSXDevice>(new ADVram(motherBoard, conf, time));
	}

	throw MSXException("Unknown device \"" + type +
	                   "\" specified in configuration");
}

} // namespace openmsx
