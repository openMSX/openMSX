// $Id$

#include "DeviceFactory.hh"
#include "XMLElement.hh"
#include "DeviceConfig.hh"
#include "MSXRam.hh"
#include "MSXPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXResetStatusRegister.hh"
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
#include "VictorFDC.hh"
#include "SanyoFDC.hh"
#include "TurboRFDC.hh"
#include "SunriseIDE.hh"
#include "GoudaSCSI.hh"
#include "MegaSCSI.hh"
#include "ESE_RAM.hh"
#include "ESE_SCC.hh"
#include "MSXMatsushita.hh"
#include "MSXVictorHC9xSystemControl.hh"
#include "MSXCielTurbo.hh"
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
#include "CliComm.hh"
#include "MSXException.hh"
#include "components.hh"

#if COMPONENT_LASERDISC
#include "PioneerLDControl.hh"
#endif

using std::auto_ptr;

namespace openmsx {

static auto_ptr<MSXDevice> createWD2793BasedFDC(const DeviceConfig& conf)
{
	auto_ptr<MSXDevice> result;
	const XMLElement* styleEl = conf.findChild("connectionstyle");
	std::string type;
	if (styleEl == NULL) {
		conf.getCliComm().printWarning(
			"WD2793 as FDC type without a connectionstyle is "
			"deprecated, please update your config file to use "
			"WD2793 with connectionstyle Philips!");
		type = "Philips";
	} else {
		type = styleEl->getData();
	}
	if (type == "Philips") {
		result.reset(new PhilipsFDC(conf));
	} else if (type == "Microsol") {
		result.reset(new MicrosolFDC(conf));
	} else if (type == "National") {
		result.reset(new NationalFDC(conf));
	} else if (type == "Sanyo") {
		result.reset(new SanyoFDC(conf));
	} else if (type == "Victor") {
		result.reset(new VictorFDC(conf));
	} else {
		throw MSXException("Unknown WD2793 FDC connection style " + type);
	}
	return result;
}

auto_ptr<MSXDevice> DeviceFactory::create(const DeviceConfig& conf)
{
	auto_ptr<MSXDevice> result;
	const std::string& type = conf.getXML()->getName();
	if (type == "PPI") {
		result.reset(new MSXPPI(conf));
	} else if (type == "RAM") {
		result.reset(new MSXRam(conf));
	} else if (type == "VDP") {
		result.reset(new VDP(conf));
	} else if (type == "E6Timer") {
		result.reset(new MSXE6Timer(conf));
	} else if (type == "ResetStatusRegister" || type == "F4Device") {
		result.reset(new MSXResetStatusRegister(conf));
	} else if (type == "TurboRPause") {
		result.reset(new MSXTurboRPause(conf));
	} else if (type == "TurboRPCM") {
		result.reset(new MSXTurboRPCM(conf));
	} else if (type == "S1985") {
		result.reset(new MSXS1985(conf));
	} else if (type == "S1990") {
		result.reset(new MSXS1990(conf));
	} else if (type == "PSG") {
		result.reset(new MSXPSG(conf));
	} else if (type == "MSX-MUSIC") {
		result.reset(new MSXMusic(conf));
	} else if (type == "FMPAC") {
		result.reset(new MSXFmPac(conf));
	} else if (type == "MSX-AUDIO") {
		result.reset(new MSXAudio(conf));
	} else if (type == "MusicModuleMIDI") {
		result.reset(new MC6850(conf));
	} else if (type == "YamahaSFG") {
		result.reset(new MSXYamahaSFG(conf));
	} else if (type == "MoonSound") {
		result.reset(new MSXMoonSound(conf));
	} else if (type == "OPL3Cartridge") {
		result.reset(new MSXOPL3Cartridge(conf));
	} else if (type == "Kanji") {
		result.reset(new MSXKanji(conf));
	} else if (type == "Bunsetsu") {
		result.reset(new MSXBunsetsu(conf));
	} else if (type == "MemoryMapper") {
		result.reset(new MSXMemoryMapper(conf));
	} else if (type == "PanasonicRAM") {
		result.reset(new PanasonicRam(conf));
	} else if (type == "RTC") {
		result.reset(new MSXRTC(conf));
	} else if (type == "PasswordCart") {
		result.reset(new PasswordCart(conf));
	} else if (type == "ROM") {
		result = RomFactory::create(conf);
	} else if (type == "PrinterPort") {
		result.reset(new MSXPrinterPort(conf));
	} else if (type == "SCCplus") { // Note: it's actually called SCC-I
		result.reset(new MSXSCCPlusCart(conf));
	} else if (type == "WD2793") {
		result = createWD2793BasedFDC(conf);
	} else if (type == "Microsol") {
		conf.getCliComm().printWarning(
			"Microsol as FDC type is deprecated, please update "
			"your config file to use WD2793 with connectionstyle "
			"Microsol!");
		result.reset(new MicrosolFDC(conf));
	} else if (type == "MB8877A") {
		conf.getCliComm().printWarning(
			"MB8877A as FDC type is deprecated, please update your "
			"config file to use WD2793 with connectionstyle National!");
		result.reset(new NationalFDC(conf));
	} else if (type == "TC8566AF") {
		result.reset(new TurboRFDC(conf));
	} else if (type == "SunriseIDE") {
		result.reset(new SunriseIDE(conf));
	} else if (type == "GoudaSCSI") {
		result.reset(new GoudaSCSI(conf));
	} else if (type == "MegaSCSI") {
		result.reset(new MegaSCSI(conf));
	} else if (type == "ESERAM") {
		result.reset(new ESE_RAM(conf));
	} else if (type == "WaveSCSI") {
		result.reset(new ESE_SCC(conf, true));
	} else if (type == "ESESCC") {
		result.reset(new ESE_SCC(conf, false));
	} else if (type == "Matsushita") {
		result.reset(new MSXMatsushita(conf));
	} else if (type == "VictorHC9xSystemControl") {
		result.reset(new MSXVictorHC9xSystemControl(conf));
	} else if (type == "CielTurbo") {
		result.reset(new MSXCielTurbo(conf));
	} else if (type == "Kanji12") {
		result.reset(new MSXKanji12(conf));
	} else if (type == "MSX-MIDI") {
		result.reset(new MSXMidi(conf));
	} else if (type == "MSX-RS232") {
		result.reset(new MSXRS232(conf));
	} else if (type == "MegaRam") {
		result.reset(new MSXMegaRam(conf));
	} else if (type == "PAC") {
		result.reset(new MSXPac(conf));
	} else if (type == "HBI55") {
		result.reset(new MSXHBI55(conf));
	} else if (type == "DebugDevice") {
		result.reset(new DebugDevice(conf));
	} else if (type == "V9990") {
		result.reset(new V9990(conf));
	} else if (type == "ADVram") {
		result.reset(new ADVram(conf));
	} else if (type == "PioneerLDControl") {
#if COMPONENT_LASERDISC
		result.reset(new PioneerLDControl(conf));
#else
		throw MSXException("Laserdisc component not compiled in");
#endif
	} else if (type == "Nowind") {
		result.reset(new NowindInterface(conf));
	} else if (type == "Mirror") {
		result.reset(new MSXMirrorDevice(conf));
	} else {
		throw MSXException("Unknown device \"" + type +
		                   "\" specified in configuration");
	}
	if (result.get()) {
		result->init();
	}
	return result;
}

static XMLElement createConfig(const std::string& name, const std::string& id)
{
	XMLElement config(name);
	config.addAttribute("id", id);
	return config;
}

auto_ptr<DummyDevice> DeviceFactory::createDummyDevice(
		const HardwareConfig& hwConf)
{
	static XMLElement xml(createConfig("Dummy", "empty"));
	return auto_ptr<DummyDevice>(new DummyDevice(
		DeviceConfig(hwConf, xml)));
}

auto_ptr<MSXDeviceSwitch> DeviceFactory::createDeviceSwitch(
		const HardwareConfig& hwConf)
{
	static XMLElement xml(createConfig("DeviceSwitch", "DeviceSwitch"));
	return auto_ptr<MSXDeviceSwitch>(new MSXDeviceSwitch(
		DeviceConfig(hwConf, xml)));
}

auto_ptr<MSXMapperIO> DeviceFactory::createMapperIO(
		const HardwareConfig& hwConf)
{
	static XMLElement xml(createConfig("MapperIO", "MapperIO"));
	return auto_ptr<MSXMapperIO>(new MSXMapperIO(
		DeviceConfig(hwConf, xml)));
}

auto_ptr<VDPIODelay> DeviceFactory::createVDPIODelay(
		const HardwareConfig& hwConf, MSXCPUInterface& cpuInterface)
{
	static XMLElement xml(createConfig("VDPIODelay", "VDPIODelay"));
	return auto_ptr<VDPIODelay>(new VDPIODelay(
		DeviceConfig(hwConf, xml), cpuInterface));
}

} // namespace openmsx
