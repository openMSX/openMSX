#include "DeviceFactory.hh"

#include "ADVram.hh"
#include "AVTFDC.hh"
#include "BeerIDE.hh"
#include "CanonFDC.hh"
#include "CanonWordProcessor.hh"
#include "Carnivore2.hh"
#include "ChakkariCopy.hh"
#include "ColecoJoystickIO.hh"
#include "ColecoSuperGameModule.hh"
#include "DalSoRiR2.hh"
#include "DebugDevice.hh"
#include "DeviceConfig.hh"
#include "DummyDevice.hh"
#include "ESE_RAM.hh"
#include "ESE_SCC.hh"
#include "FraelSwitchableROM.hh"
#include "GoudaSCSI.hh"
#include "JVCMSXMIDI.hh"
#include "MSXAudio.hh"
#include "MSXBunsetsu.hh"
#include "MSXCielTurbo.hh"
#include "MSXCliComm.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXE6Timer.hh"
#include "MSXException.hh"
#include "MSXFacMidiInterface.hh"
#include "MSXFmPac.hh"
#include "MSXHBI55.hh"
#include "MSXHiResTimer.hh"
#include "MSXKanji.hh"
#include "MSXKanji12.hh"
#include "MSXMapperIO.hh"
#include "MSXMatsushita.hh"
#include "MSXMegaRam.hh"
#include "MSXMemoryMapper.hh"
#include "MSXMidi.hh"
#include "MSXMirrorDevice.hh"
#include "MSXModem.hh"
#include "MSXMoonSound.hh"
#include "MSXMusic.hh"
#include "MSXOPL3Cartridge.hh"
#include "MSXPPI.hh"
#include "MSXPSG.hh"
#include "MSXPac.hh"
#include "MSXPrinterPort.hh"
#include "MSXRS232.hh"
#include "MSXRTC.hh"
#include "MSXRam.hh"
#include "MSXResetStatusRegister.hh"
#include "MSXS1985.hh"
#include "MSXS1990.hh"
#include "MSXSCCPlusCart.hh"
#include "MSXToshibaTcx200x.hh"
#include "MSXTurboRPCM.hh"
#include "MSXTurboRPause.hh"
#include "MSXVictorHC9xSystemControl.hh"
#include "MSXYamahaSFG.hh"
#include "MegaFlashRomSCCPlusSD.hh"
#include "MegaSCSI.hh"
#include "MicrosolFDC.hh"
#include "MusicModuleMIDI.hh"
#include "MusicalMemoryMapper.hh"
#include "NationalFDC.hh"
#include "NowindInterface.hh"
#include "PanasonicRam.hh"
#include "PasswordCart.hh"
#include "PhilipsFDC.hh"
#include "ProgrammableDevice.hh"
#include "RomFactory.hh"
#include "SC3000PPI.hh"
#include "SG1000JoystickIO.hh"
#include "SG1000Pause.hh"
#include "SNPSG.hh"
#include "SVIFDC.hh"
#include "SVIPPI.hh"
#include "SVIPSG.hh"
#include "SVIPrinterPort.hh"
#include "SanyoFDC.hh"
#include "SensorKid.hh"
#include "SpectravideoFDC.hh"
#include "SunriseIDE.hh"
#include "TalentTDC600.hh"
#include "ToshibaFDC.hh"
#include "TurboRFDC.hh"
#include "V9990.hh"
#include "VDP.hh"
#include "MSXPiDevice.hh"
#include "VDPIODelay.hh"
#include "VictorFDC.hh"
#include "Video9000.hh"
#include "XMLElement.hh"
#include "YamahaFDC.hh"
#include "YamahaSKW01.hh"

#include "one_of.hh"

#include <memory>

#include "components.hh"
#if COMPONENT_LASERDISC
#include "PioneerLDControl.hh"
#endif

namespace openmsx {

[[nodiscard]] static std::unique_ptr<MSXDevice> createWD2793BasedFDC(DeviceConfig& conf)
{
	const auto* styleEl = conf.findChild("connectionstyle");
	std::string type;
	if (!styleEl) {
		conf.getCliComm().printWarning(
			"WD2793 as FDC type without a connectionstyle is "
			"deprecated, please update your config file to use "
			"WD2793 with connectionstyle Philips!");
		type = "Philips";
	} else {
		type = styleEl->getData();
	}
	if (type == one_of("Philips", "Sony")) {
		return std::make_unique<PhilipsFDC>(conf);
	} else if (type == "Microsol") {
		return std::make_unique<MicrosolFDC>(conf);
	} else if (type == "AVT") {
		return std::make_unique<AVTFDC>(conf);
	} else if (type == "National") {
		return std::make_unique<NationalFDC>(conf);
	} else if (type == "Sanyo") {
		return std::make_unique<SanyoFDC>(conf);
	} else if (type == "Toshiba") {
		return std::make_unique<ToshibaFDC>(conf);
	} else if (type == "Canon") {
		return std::make_unique<CanonFDC>(conf);
	} else if (type == "Spectravideo") {
		return std::make_unique<SpectravideoFDC>(conf);
	} else if (type == "Victor") {
		return std::make_unique<VictorFDC>(conf);
	} else if (type == "Yamaha") {
		return std::make_unique<YamahaFDC>(conf);
	}
	throw MSXException("Unknown WD2793 FDC connection style ", type);
}

std::unique_ptr<MSXDevice> DeviceFactory::create(DeviceConfig& conf)
{
	std::unique_ptr<MSXDevice> result;
	const auto& type = conf.getXML()->getName();
	if (type == "PPI") {
		result = std::make_unique<MSXPPI>(conf);
	} else if (type == "SVIPPI") {
		result = std::make_unique<SVIPPI>(conf);
	} else if (type == "RAM") {
		result = std::make_unique<MSXRam>(conf);
	} else if (type == "VDP") {
		result = std::make_unique<VDP>(conf);
	} else if (type == "E6Timer") {
		result = std::make_unique<MSXE6Timer>(conf);
	} else if (type == "HiResTimer") {
		result = std::make_unique<MSXHiResTimer>(conf);
	} else if (type == one_of("ResetStatusRegister", "F4Device")) {
		result = std::make_unique<MSXResetStatusRegister>(conf);
	} else if (type == "TurboRPause") {
		result = std::make_unique<MSXTurboRPause>(conf);
	} else if (type == "TurboRPCM") {
		result = std::make_unique<MSXTurboRPCM>(conf);
	} else if (type == "S1985") {
		result = std::make_unique<MSXS1985>(conf);
	} else if (type == "S1990") {
		result = std::make_unique<MSXS1990>(conf);
	} else if (type == "ColecoJoystick") {
		result = std::make_unique<ColecoJoystickIO>(conf);
	} else if (type == "SuperGameModule") {
		result = std::make_unique<ColecoSuperGameModule>(conf);
	} else if (type == "SG1000Joystick") {
		result = std::make_unique<SG1000JoystickIO>(conf);
	} else if (type == "SG1000Pause") {
		result = std::make_unique<SG1000Pause>(conf);
	} else if (type == "SC3000PPI") {
		result = std::make_unique<SC3000PPI>(conf);
	} else if (type == "PSG") {
		result = std::make_unique<MSXPSG>(conf);
	} else if (type == "SVIPSG") {
		result = std::make_unique<SVIPSG>(conf);
	} else if (type == "SNPSG") {
		result = std::make_unique<SNPSG>(conf);
	} else if (type == "MSX-MUSIC") {
		result = std::make_unique<MSXMusic>(conf);
	} else if (type == "MSX-MUSIC-WX") {
		result = std::make_unique<MSXMusicWX>(conf);
	} else if (type == "FMPAC") {
		result = std::make_unique<MSXFmPac>(conf);
	} else if (type == "MSX-AUDIO") {
		result = std::make_unique<MSXAudio>(conf);
	} else if (type == "MusicModuleMIDI") {
		result = std::make_unique<MusicModuleMIDI>(conf);
	} else if (type == "JVCMSXMIDI") {
		result = std::make_unique<JVCMSXMIDI>(conf);
	} else if (type == "FACMIDIInterface") {
		result = std::make_unique<MSXFacMidiInterface>(conf);
	} else if (type == "YamahaSFG") {
		result = std::make_unique<MSXYamahaSFG>(conf);
	} else if (type == "MoonSound") {
		result = std::make_unique<MSXMoonSound>(conf);
	} else if (type == "DalSoRiR2") {
		result = std::make_unique<DalSoRiR2>(conf);
	} else if (type == "OPL3Cartridge") {
		result = std::make_unique<MSXOPL3Cartridge>(conf);
	} else if (type == "Kanji") {
		result = std::make_unique<MSXKanji>(conf);
	} else if (type == "Bunsetsu") {
		result = std::make_unique<MSXBunsetsu>(conf);
	} else if (type == "MemoryMapper") {
		result = std::make_unique<MSXMemoryMapper>(conf);
	} else if (type == "PanasonicRAM") {
		result = std::make_unique<PanasonicRam>(conf);
	} else if (type == "RTC") {
		result = std::make_unique<MSXRTC>(conf);
	} else if (type == "PasswordCart") {
		result = std::make_unique<PasswordCart>(conf);
	} else if (type == "ROM") {
		result = RomFactory::create(conf);
	} else if (type == "PrinterPort") {
		result = std::make_unique<MSXPrinterPort>(conf);
	} else if (type == "SVIPrinterPort") {
		result = std::make_unique<SVIPrinterPort>(conf);
	} else if (type == "SCCplus") { // Note: it's actually called SCC-I
		result = std::make_unique<MSXSCCPlusCart>(conf);
	} else if (type == one_of("WD2793", "WD1770")) {
		result = createWD2793BasedFDC(conf);
	} else if (type == "Microsol") {
		conf.getCliComm().printWarning(
			"Microsol as FDC type is deprecated, please update "
			"your config file to use WD2793 with connectionstyle "
			"Microsol!");
		result = std::make_unique<MicrosolFDC>(conf);
	} else if (type == "MB8877A") {
		conf.getCliComm().printWarning(
			"MB8877A as FDC type is deprecated, please update your "
			"config file to use WD2793 with connectionstyle National!");
		result = std::make_unique<NationalFDC>(conf);
	} else if (type == "TC8566AF") {
		result = std::make_unique<TurboRFDC>(conf);
	} else if (type == "TDC600") {
		result = std::make_unique<TalentTDC600>(conf);
	} else if (type == "ToshibaTCX-200x") {
		result = std::make_unique<MSXToshibaTcx200x>(conf);
	} else if (type == "SVIFDC") {
		result = std::make_unique<SVIFDC>(conf);
	} else if (type == "BeerIDE") {
		result = std::make_unique<BeerIDE>(conf);
	} else if (type == "SunriseIDE") {
		result = std::make_unique<SunriseIDE>(conf);
	} else if (type == "GoudaSCSI") {
		result = std::make_unique<GoudaSCSI>(conf);
	} else if (type == "MegaSCSI") {
		result = std::make_unique<MegaSCSI>(conf);
	} else if (type == "ESERAM") {
		result = std::make_unique<ESE_RAM>(conf);
	} else if (type == "WaveSCSI") {
		result = std::make_unique<ESE_SCC>(conf, true);
	} else if (type == "ESESCC") {
		result = std::make_unique<ESE_SCC>(conf, false);
	} else if (type == "Matsushita") {
		result = std::make_unique<MSXMatsushita>(conf);
	} else if (type == "VictorHC9xSystemControl") {
		result = std::make_unique<MSXVictorHC9xSystemControl>(conf);
	} else if (type == "CielTurbo") {
		result = std::make_unique<MSXCielTurbo>(conf);
	} else if (type == "Kanji12") {
		result = std::make_unique<MSXKanji12>(conf);
	} else if (type == "MSX-MIDI") {
		result = std::make_unique<MSXMidi>(conf);
	} else if (type == "MSX-Modem") {
		result = std::make_unique<MSXModem>(conf);
	} else if (type == "MSX-RS232") {
		result = std::make_unique<MSXRS232>(conf);
	} else if (type == "MegaRam") {
		result = std::make_unique<MSXMegaRam>(conf);
	} else if (type == "PAC") {
		result = std::make_unique<MSXPac>(conf);
	} else if (type == "HBI55") {
		result = std::make_unique<MSXHBI55>(conf);
	} else if (type == "ProgrammableDevice") {
		result = std::make_unique<ProgrammableDevice>(conf);
	} else if (type == "DebugDevice") {
		result = std::make_unique<DebugDevice>(conf);
	} else if (type == "V9990") {
		result = std::make_unique<V9990>(conf);
	} else if (type == "Video9000") {
		result = std::make_unique<Video9000>(conf);
	} else if (type == "ADVram") {
		result = std::make_unique<ADVram>(conf);
	} else if (type == "PioneerLDControl") {
#if COMPONENT_LASERDISC
		result = std::make_unique<PioneerLDControl>(conf);
#else
		throw MSXException("Laserdisc component not compiled in");
#endif
	} else if (type == "Nowind") {
		result = std::make_unique<NowindInterface>(conf);
	} else if (type == "Mirror") {
		result = std::make_unique<MSXMirrorDevice>(conf);
	} else if (type == "SensorKid") {
		result = std::make_unique<SensorKid>(conf);
	} else if (type == "FraelSwitchableROM") {
		result = std::make_unique<FraelSwitchableROM>(conf);
	} else if (type == "ChakkariCopy") {
		result = std::make_unique<ChakkariCopy>(conf);
	} else if (type == "CanonWordProcessor") {
		result = std::make_unique<CanonWordProcessor>(conf);
	} else if (type == "MegaFlashRomSCCPlusSD") {
		result = std::make_unique<MegaFlashRomSCCPlusSD>(conf);
	} else if (type == "MusicalMemoryMapper") {
		result = std::make_unique<MusicalMemoryMapper>(conf);
	} else if (type == "Carnivore2") {
		result = std::make_unique<Carnivore2>(conf);
	} else if (type == "YamahaSKW01") {
		result = std::make_unique<YamahaSKW01>(conf);
	} else if (type == one_of("T7775", "T7937", "T9763", "T9769")) {
		// Ignore for now. We might want to create a real device for it later.
	} else if (type == "MSXPiDevice") {
		result = std::make_unique<MSXPiDevice>(conf);
	} else {
		throw MSXException("Unknown device \"", type,
		                   "\" specified in configuration");
	}
	if (result) result->init();
	return result;
}

[[nodiscard]] static XMLElement& createConfig(const char* name, const char* id)
{
	auto& doc = XMLDocument::getStaticDocument();
	auto* config = doc.allocateElement(name);
	config->setFirstAttribute(doc.allocateAttribute("id", id));
	return *config;
}

std::unique_ptr<DummyDevice> DeviceFactory::createDummyDevice(HardwareConfig& hwConf)
{
	static XMLElement& xml(createConfig("Dummy", ""));
	return std::make_unique<DummyDevice>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<MSXDeviceSwitch> DeviceFactory::createDeviceSwitch(HardwareConfig& hwConf)
{
	static XMLElement& xml(createConfig("DeviceSwitch", "DeviceSwitch"));
	return std::make_unique<MSXDeviceSwitch>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<MSXMapperIO> DeviceFactory::createMapperIO(HardwareConfig& hwConf)
{
	static XMLElement& xml(createConfig("MapperIO", "MapperIO"));
	return std::make_unique<MSXMapperIO>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<VDPIODelay> DeviceFactory::createVDPIODelay(
		HardwareConfig& hwConf, MSXCPUInterface& cpuInterface)
{
	static XMLElement& xml(createConfig("VDPIODelay", "VDPIODelay"));
	return std::make_unique<VDPIODelay>(DeviceConfig(hwConf, xml), cpuInterface);
}

} // namespace openmsx
