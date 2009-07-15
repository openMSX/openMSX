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
#include "MSXOPL3Cartridge.hh"
#include "MSXYamahaSFG.hh"
#include "MC6850.hh"
#include "MSXKanji.hh"
#include "MSXBunsetsu.hh"
#include "MSXMemoryMapper.hh"
#include "PanasonicRam.hh"
#include "MSXRTC.hh"
#include "PasswordCart.hh"
#include "RomFactory.hh"
#include "MSXPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "NationalFDC.hh"
#include "TurboRFDC.hh"
#include "SunriseIDE.hh"
#include "GoudaSCSI.hh"
#include "MegaSCSI.hh"
#include "ESE_RAM.hh"
#include "ESE_SCC.hh"
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
#include "NowindInterface.hh"
#include "MSXMirrorDevice.hh"
#include "DummyDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXMapperIO.hh"
#include "VDPIODelay.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#ifdef COMPONENT_LASERDISC
#include "PioneerLDControl.hh"
#endif

namespace openmsx {

std::auto_ptr<MSXDevice> DeviceFactory::create(
	MSXMotherBoard& motherBoard, const HardwareConfig& hwConf,
	const XMLElement& conf)
{
	std::auto_ptr<MSXDevice> result;
	const std::string& type = conf.getName();
	if (type == "PPI") {
		result.reset(new MSXPPI(motherBoard, conf));
	} else if (type == "RAM") {
		result.reset(new MSXRam(motherBoard, conf));
	} else if (type == "VDP") {
		result.reset(new VDP(motherBoard, conf));
	} else if (type == "E6Timer") {
		result.reset(new MSXE6Timer(motherBoard, conf));
	} else if (type == "F4Device") {
		result.reset(new MSXF4Device(motherBoard, conf));
	} else if (type == "TurboRPause") {
		result.reset(new MSXTurboRPause(motherBoard, conf));
	} else if (type == "TurboRPCM") {
		result.reset(new MSXTurboRPCM(motherBoard, conf));
	} else if (type == "S1985") {
		result.reset(new MSXS1985(motherBoard, conf));
	} else if (type == "S1990") {
		result.reset(new MSXS1990(motherBoard, conf));
	} else if (type == "PSG") {
		result.reset(new MSXPSG(motherBoard, conf));
	} else if (type == "MSX-MUSIC") {
		result.reset(new MSXMusic(motherBoard, conf));
	} else if (type == "FMPAC") {
		result.reset(new MSXFmPac(motherBoard, conf));
	} else if (type == "MSX-AUDIO") {
		result.reset(new MSXAudio(motherBoard, conf));
	} else if (type == "MusicModuleMIDI") {
		result.reset(new MC6850(motherBoard, conf));
	} else if (type == "YamahaSFG") {
		result.reset(new MSXYamahaSFG(motherBoard, conf));
	} else if (type == "MoonSound") {
		result.reset(new MSXMoonSound(motherBoard, conf));
	} else if (type == "OPL3Cartridge") {
		result.reset(new MSXOPL3Cartridge(motherBoard, conf));
	} else if (type == "Kanji") {
		result.reset(new MSXKanji(motherBoard, conf));
	} else if (type == "Bunsetsu") {
		result.reset(new MSXBunsetsu(motherBoard, conf));
	} else if (type == "MemoryMapper") {
		result.reset(new MSXMemoryMapper(motherBoard, conf));
	} else if (type == "PanasonicRAM") {
		result.reset(new PanasonicRam(motherBoard, conf));
	} else if (type == "RTC") {
		result.reset(new MSXRTC(motherBoard, conf));
	} else if (type == "PasswordCart") {
		result.reset(new PasswordCart(motherBoard, conf));
	} else if (type == "ROM") {
		result = RomFactory::create(motherBoard, conf);
	} else if (type == "PrinterPort") {
		result.reset(new MSXPrinterPort(motherBoard, conf));
	} else if (type == "SCCplus") { // Note: it's actually called SCC-I
		result.reset(new MSXSCCPlusCart(motherBoard, conf));
	} else if (type == "WD2793") {
		result.reset(new PhilipsFDC(motherBoard, conf));
	} else if (type == "Microsol") {
		result.reset(new MicrosolFDC(motherBoard, conf));
	} else if (type == "MB8877A") {
		result.reset(new NationalFDC(motherBoard, conf));
	} else if (type == "TC8566AF") {
		result.reset(new TurboRFDC(motherBoard, conf));
	} else if (type == "SunriseIDE") {
		result.reset(new SunriseIDE(motherBoard, conf));
	} else if (type == "GoudaSCSI") {
		result.reset(new GoudaSCSI(motherBoard, conf));
	} else if (type == "MegaSCSI") {
		result.reset(new MegaSCSI(motherBoard, conf));
	} else if (type == "ESERAM") {
		result.reset(new ESE_RAM(motherBoard, conf));
	} else if (type == "WaveSCSI") {
		result.reset(new ESE_SCC(motherBoard, conf, true));
	} else if (type == "ESESCC") {
		result.reset(new ESE_SCC(motherBoard, conf, false));
	} else if (type == "Matsushita") {
		result.reset(new MSXMatsushita(motherBoard, conf));
	} else if (type == "Kanji12") {
		result.reset(new MSXKanji12(motherBoard, conf));
	} else if (type == "MSX-MIDI") {
		result.reset(new MSXMidi(motherBoard, conf));
	} else if (type == "MSX-RS232") {
		result.reset(new MSXRS232(motherBoard, conf));
	} else if (type == "MegaRam") {
		result.reset(new MSXMegaRam(motherBoard, conf));
	} else if (type == "PAC") {
		result.reset(new MSXPac(motherBoard, conf));
	} else if (type == "HBI55") {
		result.reset(new MSXHBI55(motherBoard, conf));
	} else if (type == "DebugDevice") {
		result.reset(new DebugDevice(motherBoard, conf));
	} else if (type == "V9990") {
		result.reset(new V9990(motherBoard, conf));
	} else if (type == "ADVram") {
		result.reset(new ADVram(motherBoard, conf));
	} else if (type == "PioneerLDControl") {
#ifdef COMPONENT_LASERDISC
		result.reset(new PioneerLDControl(motherBoard, conf));
#else
		throw MSXException("Laserdisc component not compiled in");
#endif
	} else if (type == "Nowind") {
		result.reset(new NowindInterface(motherBoard, conf));
	} else if (type == "Mirror") {
		result.reset(new MSXMirrorDevice(motherBoard, conf));
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
	return config;
}

std::auto_ptr<DummyDevice> DeviceFactory::createDummyDevice(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("Dummy", "empty"));
	std::auto_ptr<DummyDevice> result(
		new DummyDevice(motherBoard, config));
	result->init(*motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<MSXDeviceSwitch> DeviceFactory::createDeviceSwitch(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("DeviceSwitch", "DeviceSwitch"));
	std::auto_ptr<MSXDeviceSwitch> result(
		new MSXDeviceSwitch(motherBoard, config));
	result->init(*motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<MSXMapperIO> DeviceFactory::createMapperIO(
		MSXMotherBoard& motherBoard)
{
	static XMLElement config(createConfig("MapperIO", "MapperIO"));
	std::auto_ptr<MSXMapperIO> result(new MSXMapperIO(motherBoard, config));
	result->init(*motherBoard.getMachineConfig());
	return result;
}

std::auto_ptr<VDPIODelay> DeviceFactory::createVDPIODelay(
		MSXMotherBoard& motherBoard, MSXCPUInterface& cpuInterface)
{
	static XMLElement config(createConfig("VDPIODelay", "VDPIODelay"));
	std::auto_ptr<VDPIODelay> result(
		new VDPIODelay(motherBoard, config, cpuInterface));
	result->init(*motherBoard.getMachineConfig());
	return result;
}

} // namespace openmsx
