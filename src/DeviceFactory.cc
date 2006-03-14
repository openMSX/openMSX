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
#include "DummyDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXMapperIO.hh"
#include "VDPIODelay.hh"
#include "MSXMotherBoard.hh"
#include "MachineConfig.hh"
#include "EmuTime.hh"
#include "FileContext.hh"

namespace openmsx {

std::auto_ptr<MSXDevice> DeviceFactory::create(
	MSXMotherBoard& motherBoard, const HardwareConfig& hwConf,
	const XMLElement& conf, const EmuTime& time)
{
	std::auto_ptr<MSXDevice> result;
	const std::string& type = conf.getName();
	if (type == "PPI") {
		result.reset(new MSXPPI(motherBoard, conf, time));
	} else if (type == "RAM") {
		result.reset(new MSXRam(motherBoard, conf, time));
	} else if (type == "VDP") {
		result.reset(new VDP(motherBoard, conf, time));
	} else if (type == "E6Timer") {
		result.reset(new MSXE6Timer(motherBoard, conf, time));
	} else if (type == "F4Device") {
		result.reset(new MSXF4Device(motherBoard, conf, time));
	} else if (type == "TurboRLeds") {
		// deprecated, remove in next version
	} else if (type == "TurboRPause") {
		result.reset(new MSXTurboRPause(motherBoard, conf, time));
	} else if (type == "TurboRPCM") {
		result.reset(new MSXTurboRPCM(motherBoard, conf, time));
	} else if (type == "S1985") {
		result.reset(new MSXS1985(motherBoard, conf, time));
	} else if (type == "S1990") {
		result.reset(new MSXS1990(motherBoard, conf, time));
	} else if (type == "PSG") {
		result.reset(new MSXPSG(motherBoard, conf, time));
	} else if (type == "MSX-MUSIC") {
		result.reset(new MSXMusic(motherBoard, conf, time));
	} else if (type == "FMPAC") {
		result.reset(new MSXFmPac(motherBoard, conf, time));
	} else if (type == "MSX-AUDIO") {
		result.reset(new MSXAudio(motherBoard, conf, time));
	} else if (type == "MusicModuleMIDI") {
		result.reset(new MC6850(motherBoard, conf, time));
	} else if (type == "MoonSound") {
		result.reset(new MSXMoonSound(motherBoard, conf, time));
	} else if (type == "Kanji") {
		result.reset(new MSXKanji(motherBoard, conf, time));
	} else if (type == "Bunsetsu") {
		result.reset(new MSXBunsetsu(motherBoard, conf, time));
	} else if (type == "MemoryMapper") {
		result.reset(new MSXMemoryMapper(motherBoard, conf, time));
	} else if (type == "PanasonicRAM") {
		result.reset(new PanasonicRam(motherBoard, conf, time));
	} else if (type == "RTC") {
		result.reset(new MSXRTC(motherBoard, conf, time));
	} else if (type == "ROM") {
		result = RomFactory::create(motherBoard, conf, time);
	} else if (type == "PrinterPort") {
		result.reset(new MSXPrinterPort(motherBoard, conf, time));
	} else if (type == "SCCplus") { // Note: it's actually called SCC-I
		result.reset(new MSXSCCPlusCart(motherBoard, conf, time));
	} else if (type == "WD2793") {
		result.reset(new PhilipsFDC(motherBoard, conf, time));
	} else if (type == "Microsol") {
		result.reset(new MicrosolFDC(motherBoard, conf, time));
	} else if (type == "MB8877A") {
		result.reset(new NationalFDC(motherBoard, conf, time));
	} else if (type == "TC8566AF") {
		result.reset(new TurboRFDC(motherBoard, conf, time));
	} else if (type == "SunriseIDE") {
		result.reset(new SunriseIDE(motherBoard, conf, time));
	} else if (type == "Matsushita") {
		result.reset(new MSXMatsushita(motherBoard, conf, time));
	} else if (type == "Kanji12") {
		result.reset(new MSXKanji12(motherBoard, conf, time));
	} else if (type == "MSX-MIDI") {
		result.reset(new MSXMidi(motherBoard, conf, time));
	} else if (type == "MSX-RS232") {
		result.reset(new MSXRS232(motherBoard, conf, time));
	} else if (type == "MegaRam") {
		result.reset(new MSXMegaRam(motherBoard, conf, time));
	} else if (type == "PAC") {
		result.reset(new MSXPac(motherBoard, conf, time));
	} else if (type == "HBI55") {
		result.reset(new MSXHBI55(motherBoard, conf, time));
	} else if (type == "DebugDevice") {
		result.reset(new DebugDevice(motherBoard, conf, time));
	} else if (type == "V9990") {
		result.reset(new V9990(motherBoard, conf, time));
	} else if (type == "ADVram") {
		result.reset(new ADVram(motherBoard, conf, time));
	} else {
		throw MSXException("Unknown device \"" + type +
		                   "\" specified in configuration");
	}
	if (result.get()) {
		result->init(hwConf);
	}
	return result;
}

static XMLElement createConfig(const std::string& name, const std::string& id)
{
	XMLElement config(name);
	config.addAttribute("id", id);
	config.setFileContext(std::auto_ptr<FileContext>(
		new SystemFileContext()));
	return config;
}

std::auto_ptr<DummyDevice> DeviceFactory::createDummyDevice(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("Dummy", "empty"));
	std::auto_ptr<DummyDevice> result(
		new DummyDevice(motherBoard, config, EmuTime::zero));
	result->init(motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<MSXDeviceSwitch> DeviceFactory::createDeviceSwitch(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("DeviceSwitch", "DeviceSwitch"));
	std::auto_ptr<MSXDeviceSwitch> result(
		new MSXDeviceSwitch(motherBoard, config, EmuTime::zero));
	result->init(motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<MSXMapperIO> DeviceFactory::createMapperIO(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("MapperIO", "MapperIO"));
	std::auto_ptr<MSXMapperIO> result(
		new MSXMapperIO(motherBoard, config, EmuTime::zero));
	result->init(motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<VDPIODelay> DeviceFactory::createVDPIODelay(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("VDPIODelay", "VDPIODelay"));
	std::auto_ptr<VDPIODelay> result(
		new VDPIODelay(motherBoard, config, EmuTime::zero));
	result->init(motherBoard.getMachineConfig());
	return result;
}

} // namespace openmsx
