#include "DeviceFactory.hh"
#include "XMLElement.hh"
#include "DeviceConfig.hh"
#include "ChakkariCopy.hh"
#include "CanonWordProcessor.hh"
#include "FraelSwitchableROM.hh"
#include "MSXRam.hh"
#include "MSXPPI.hh"
#include "SVIPPI.hh"
#include "VDP.hh"
#include "MSXE6Timer.hh"
#include "MSXFacMidiInterface.hh"
#include "MSXHiResTimer.hh"
#include "MSXResetStatusRegister.hh"
#include "MSXTurboRPause.hh"
#include "MSXTurboRPCM.hh"
#include "MSXS1985.hh"
#include "MSXS1990.hh"
#include "ColecoJoystickIO.hh"
#include "ColecoSuperGameModule.hh"
#include "SG1000JoystickIO.hh"
#include "SG1000Pause.hh"
#include "SC3000PPI.hh"
#include "MSXPSG.hh"
#include "SVIPSG.hh"
#include "SNPSG.hh"
#include "MSXMusic.hh"
#include "MSXFmPac.hh"
#include "MSXAudio.hh"
#include "MSXMoonSound.hh"
#include "DalSoRiR2.hh"
#include "MSXOPL3Cartridge.hh"
#include "MSXYamahaSFG.hh"
#include "MusicModuleMIDI.hh"
#include "JVCMSXMIDI.hh"
#include "MSXKanji.hh"
#include "MSXBunsetsu.hh"
#include "MSXMemoryMapper.hh"
#include "MSXToshibaTcx200x.hh"
#include "MegaFlashRomSCCPlusSD.hh"
#include "MusicalMemoryMapper.hh"
#include "Carnivore2.hh"
#include "PanasonicRam.hh"
#include "MSXRTC.hh"
#include "PasswordCart.hh"
#include "RomFactory.hh"
#include "MSXPrinterPort.hh"
#include "SVIPrinterPort.hh"
#include "MSXSCCPlusCart.hh"
#include "PhilipsFDC.hh"
#include "MicrosolFDC.hh"
#include "AVTFDC.hh"
#include "NationalFDC.hh"
#include "VictorFDC.hh"
#include "SanyoFDC.hh"
#include "ToshibaFDC.hh"
#include "CanonFDC.hh"
#include "SpectravideoFDC.hh"
#include "TalentTDC600.hh"
#include "TurboRFDC.hh"
#include "SVIFDC.hh"
#include "YamahaFDC.hh"
#include "SunriseIDE.hh"
#include "BeerIDE.hh"
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
#include "MSXModem.hh"
#include "MSXMegaRam.hh"
#include "MSXPac.hh"
#include "MSXHBI55.hh"
#include "DebugDevice.hh"
#include "V9990.hh"
#include "Video9000.hh"
#include "RomAscii16X.hh"
#include "ADVram.hh"
#include "NowindInterface.hh"
#include "MSXMirrorDevice.hh"
#include "DummyDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXMapperIO.hh"
#include "VDPIODelay.hh"
#include "SensorKid.hh"
#include "YamahaSKW01.hh"
#include "MSXCliComm.hh"
#include "MSXException.hh"
#include "components.hh"
#include "one_of.hh"
#include <memory>

#if COMPONENT_LASERDISC
#include "PioneerLDControl.hh"
#endif

using std::make_unique;

namespace openmsx {

[[nodiscard]] static std::unique_ptr<MSXDevice> createWD2793BasedFDC(const DeviceConfig& conf)
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
		return make_unique<PhilipsFDC>(conf);
	} else if (type == "Microsol") {
		return make_unique<MicrosolFDC>(conf);
	} else if (type == "AVT") {
		return make_unique<AVTFDC>(conf);
	} else if (type == "National") {
		return make_unique<NationalFDC>(conf);
	} else if (type == "Sanyo") {
		return make_unique<SanyoFDC>(conf);
	} else if (type == "Toshiba") {
		return make_unique<ToshibaFDC>(conf);
	} else if (type == "Canon") {
		return make_unique<CanonFDC>(conf);
	} else if (type == "Spectravideo") {
		return make_unique<SpectravideoFDC>(conf);
	} else if (type == "Victor") {
		return make_unique<VictorFDC>(conf);
	} else if (type == "Yamaha") {
		return make_unique<YamahaFDC>(conf);
	}
	throw MSXException("Unknown WD2793 FDC connection style ", type);
}

std::unique_ptr<MSXDevice> DeviceFactory::create(const DeviceConfig& conf)
{
	std::unique_ptr<MSXDevice> result;
	const auto& type = conf.getXML()->getName();
	if (type == "PPI") {
		result = make_unique<MSXPPI>(conf);
	} else if (type == "SVIPPI") {
		result = make_unique<SVIPPI>(conf);
	} else if (type == "RAM") {
		result = make_unique<MSXRam>(conf);
	} else if (type == "VDP") {
		result = make_unique<VDP>(conf);
	} else if (type == "E6Timer") {
		result = make_unique<MSXE6Timer>(conf);
	} else if (type == "HiResTimer") {
		result = make_unique<MSXHiResTimer>(conf);
	} else if (type == one_of("ResetStatusRegister", "F4Device")) {
		result = make_unique<MSXResetStatusRegister>(conf);
	} else if (type == "TurboRPause") {
		result = make_unique<MSXTurboRPause>(conf);
	} else if (type == "TurboRPCM") {
		result = make_unique<MSXTurboRPCM>(conf);
	} else if (type == "S1985") {
		result = make_unique<MSXS1985>(conf);
	} else if (type == "S1990") {
		result = make_unique<MSXS1990>(conf);
	} else if (type == "ColecoJoystick") {
		result = make_unique<ColecoJoystickIO>(conf);
	} else if (type == "SuperGameModule") {
		result = make_unique<ColecoSuperGameModule>(conf);
	} else if (type == "SG1000Joystick") {
		result = make_unique<SG1000JoystickIO>(conf);
	} else if (type == "SG1000Pause") {
		result = make_unique<SG1000Pause>(conf);
	} else if (type == "SC3000PPI") {
		result = make_unique<SC3000PPI>(conf);
	} else if (type == "PSG") {
		result = make_unique<MSXPSG>(conf);
	} else if (type == "SVIPSG") {
		result = make_unique<SVIPSG>(conf);
	} else if (type == "SNPSG") {
		result = make_unique<SNPSG>(conf);
	} else if (type == "MSX-MUSIC") {
		result = make_unique<MSXMusic>(conf);
	} else if (type == "MSX-MUSIC-WX") {
		result = make_unique<MSXMusicWX>(conf);
	} else if (type == "FMPAC") {
		result = make_unique<MSXFmPac>(conf);
	} else if (type == "MSX-AUDIO") {
		result = make_unique<MSXAudio>(conf);
	} else if (type == "MusicModuleMIDI") {
		result = make_unique<MusicModuleMIDI>(conf);
	} else if (type == "JVCMSXMIDI") {
		result = make_unique<JVCMSXMIDI>(conf);
	} else if (type == "FACMIDIInterface") {
		result = make_unique<MSXFacMidiInterface>(conf);
	} else if (type == "YamahaSFG") {
		result = make_unique<MSXYamahaSFG>(conf);
	} else if (type == "MoonSound") {
		result = make_unique<MSXMoonSound>(conf);
	} else if (type == "DalSoRiR2") {
		result = make_unique<DalSoRiR2>(conf);
	} else if (type == "OPL3Cartridge") {
		result = make_unique<MSXOPL3Cartridge>(conf);
	} else if (type == "Kanji") {
		result = make_unique<MSXKanji>(conf);
	} else if (type == "Bunsetsu") {
		result = make_unique<MSXBunsetsu>(conf);
	} else if (type == "MemoryMapper") {
		result = make_unique<MSXMemoryMapper>(conf);
	} else if (type == "PanasonicRAM") {
		result = make_unique<PanasonicRam>(conf);
	} else if (type == "RTC") {
		result = make_unique<MSXRTC>(conf);
	} else if (type == "PasswordCart") {
		result = make_unique<PasswordCart>(conf);
	} else if (type == "ROM") {
		result = RomFactory::create(conf);
	} else if (type == "PrinterPort") {
		result = make_unique<MSXPrinterPort>(conf);
	} else if (type == "SVIPrinterPort") {
		result = make_unique<SVIPrinterPort>(conf);
	} else if (type == "SCCplus") { // Note: it's actually called SCC-I
		result = make_unique<MSXSCCPlusCart>(conf);
	} else if (type == one_of("WD2793", "WD1770")) {
		result = createWD2793BasedFDC(conf);
	} else if (type == "Microsol") {
		conf.getCliComm().printWarning(
			"Microsol as FDC type is deprecated, please update "
			"your config file to use WD2793 with connectionstyle "
			"Microsol!");
		result = make_unique<MicrosolFDC>(conf);
	} else if (type == "MB8877A") {
		conf.getCliComm().printWarning(
			"MB8877A as FDC type is deprecated, please update your "
			"config file to use WD2793 with connectionstyle National!");
		result = make_unique<NationalFDC>(conf);
	} else if (type == "TC8566AF") {
		result = make_unique<TurboRFDC>(conf);
	} else if (type == "TDC600") {
		result = make_unique<TalentTDC600>(conf);
	} else if (type == "ToshibaTCX-200x") {
		result = make_unique<MSXToshibaTcx200x>(conf);
	} else if (type == "SVIFDC") {
		result = make_unique<SVIFDC>(conf);
	} else if (type == "BeerIDE") {
		result = make_unique<BeerIDE>(conf);
	} else if (type == "SunriseIDE") {
		result = make_unique<SunriseIDE>(conf);
	} else if (type == "GoudaSCSI") {
		result = make_unique<GoudaSCSI>(conf);
	} else if (type == "MegaSCSI") {
		result = make_unique<MegaSCSI>(conf);
	} else if (type == "ESERAM") {
		result = make_unique<ESE_RAM>(conf);
	} else if (type == "WaveSCSI") {
		result = make_unique<ESE_SCC>(conf, true);
	} else if (type == "ESESCC") {
		result = make_unique<ESE_SCC>(conf, false);
	} else if (type == "Matsushita") {
		result = make_unique<MSXMatsushita>(conf);
	} else if (type == "VictorHC9xSystemControl") {
		result = make_unique<MSXVictorHC9xSystemControl>(conf);
	} else if (type == "CielTurbo") {
		result = make_unique<MSXCielTurbo>(conf);
	} else if (type == "Kanji12") {
		result = make_unique<MSXKanji12>(conf);
	} else if (type == "MSX-MIDI") {
		result = make_unique<MSXMidi>(conf);
	} else if (type == "MSX-Modem") {
		result = make_unique<MSXModem>(conf);
	} else if (type == "MSX-RS232") {
		result = make_unique<MSXRS232>(conf);
	} else if (type == "MegaRam") {
		result = make_unique<MSXMegaRam>(conf);
	} else if (type == "PAC") {
		result = make_unique<MSXPac>(conf);
	} else if (type == "HBI55") {
		result = make_unique<MSXHBI55>(conf);
	} else if (type == "DebugDevice") {
		result = make_unique<DebugDevice>(conf);
	} else if (type == "V9990") {
		result = make_unique<V9990>(conf);
	} else if (type == "Video9000") {
		result = make_unique<Video9000>(conf);
	} else if (type == "ADVram") {
		result = make_unique<ADVram>(conf);
	} else if (type == "PioneerLDControl") {
#if COMPONENT_LASERDISC
		result = make_unique<PioneerLDControl>(conf);
#else
		throw MSXException("Laserdisc component not compiled in");
#endif
	} else if (type == "Nowind") {
		result = make_unique<NowindInterface>(conf);
	} else if (type == "Mirror") {
		result = make_unique<MSXMirrorDevice>(conf);
	} else if (type == "SensorKid") {
		result = make_unique<SensorKid>(conf);
	} else if (type == "FraelSwitchableROM") {
		result = make_unique<FraelSwitchableROM>(conf);
	} else if (type == "ChakkariCopy") {
		result = make_unique<ChakkariCopy>(conf);
	} else if (type == "CanonWordProcessor") {
		result = make_unique<CanonWordProcessor>(conf);
	} else if (type == "MegaFlashRomSCCPlusSD") {
		result = make_unique<MegaFlashRomSCCPlusSD>(conf);
	} else if (type == "MusicalMemoryMapper") {
		result = make_unique<MusicalMemoryMapper>(conf);
	} else if (type == "Carnivore2") {
		result = make_unique<Carnivore2>(conf);
	} else if (type == "YamahaSKW01") {
		result = make_unique<YamahaSKW01>(conf);
	} else if (type == one_of("T7775", "T7937", "T9763", "T9769")) {
		// Ignore for now. We might want to create a real device for it later.
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

std::unique_ptr<DummyDevice> DeviceFactory::createDummyDevice(
		const HardwareConfig& hwConf)
{
	static const XMLElement& xml(createConfig("Dummy", ""));
	return make_unique<DummyDevice>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<MSXDeviceSwitch> DeviceFactory::createDeviceSwitch(
		const HardwareConfig& hwConf)
{
	static const XMLElement& xml(createConfig("DeviceSwitch", "DeviceSwitch"));
	return make_unique<MSXDeviceSwitch>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<MSXMapperIO> DeviceFactory::createMapperIO(
		const HardwareConfig& hwConf)
{
	static const XMLElement& xml(createConfig("MapperIO", "MapperIO"));
	return make_unique<MSXMapperIO>(DeviceConfig(hwConf, xml));
}

std::unique_ptr<VDPIODelay> DeviceFactory::createVDPIODelay(
		const HardwareConfig& hwConf, MSXCPUInterface& cpuInterface)
{
	static const XMLElement& xml(createConfig("VDPIODelay", "VDPIODelay"));
	return make_unique<VDPIODelay>(DeviceConfig(hwConf, xml), cpuInterface);
}

} // namespace openmsx
