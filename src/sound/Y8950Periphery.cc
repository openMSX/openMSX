#include "Y8950Periphery.hh"
#include "Y8950.hh"
#include "MSXAudio.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "CacheLine.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "BooleanSetting.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "DeviceConfig.hh"
#include "serialize.hh"
#include <memory>
#include <string>

namespace openmsx {

// Subclass declarations:

class MusicModulePeriphery final : public Y8950Periphery
{
public:
	explicit MusicModulePeriphery(MSXAudio& audio);
	void write(uint4_t outputs, uint4_t values, EmuTime::param time) override;
	[[nodiscard]] uint4_t read(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& /*ar*/, unsigned /*version*/) {
		// nothing
	}

private:
	MSXAudio& audio;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, MusicModulePeriphery, "MusicModule");

class PanasonicAudioPeriphery final : public Y8950Periphery
{
public:
	PanasonicAudioPeriphery(
		MSXAudio& audio, DeviceConfig& config,
		const std::string& soundDeviceName);
	PanasonicAudioPeriphery(const PanasonicAudioPeriphery&) = delete;
	PanasonicAudioPeriphery(PanasonicAudioPeriphery&&) = delete;
	PanasonicAudioPeriphery& operator=(const PanasonicAudioPeriphery&) = delete;
	PanasonicAudioPeriphery& operator=(PanasonicAudioPeriphery&&) = delete;
	~PanasonicAudioPeriphery() override;

	void reset() override;

	void write(uint4_t outputs, uint4_t values, EmuTime::param time) override;
	[[nodiscard]] uint4_t read(EmuTime::param time) override;

	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime::param time) const override;
	void writeMem(uint16_t address, uint8_t value, EmuTime::param time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] uint8_t* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(uint8_t value);
	void setIOPorts(uint8_t value);
	void setIOPortsHelper(uint8_t base, bool enable);

private:
	MSXAudio& audio;
	BooleanSetting swSwitch;
	Ram ram;
	Rom rom;
	uint8_t bankSelect;
	uint8_t ioPorts = 0;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, PanasonicAudioPeriphery, "Panasonic");

class ToshibaAudioPeriphery final : public Y8950Periphery
{
public:
	explicit ToshibaAudioPeriphery(MSXAudio& audio);
	void write(uint4_t outputs, uint4_t values, EmuTime::param time) override;
	[[nodiscard]] uint4_t read(EmuTime::param time) override;
	void setSPOFF(bool value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& /*ar*/, unsigned /*version*/) {
		// nothing
	}

private:
	MSXAudio& audio;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, ToshibaAudioPeriphery, "Toshiba");


// Base class implementation:

void Y8950Periphery::reset()
{
	// nothing
}

void Y8950Periphery::setSPOFF(bool /*value*/, EmuTime::param /*time*/)
{
	// nothing
}

uint8_t Y8950Periphery::readMem(uint16_t address, EmuTime::param time)
{
	// by default do same as peekMem()
	return peekMem(address, time);
}
uint8_t Y8950Periphery::peekMem(uint16_t /*address*/, EmuTime::param /*time*/) const
{
	return 0xFF;
}
void Y8950Periphery::writeMem(uint16_t /*address*/, uint8_t /*value*/, EmuTime::param /*time*/)
{
	// nothing
}
const uint8_t* Y8950Periphery::getReadCacheLine(uint16_t /*address*/) const
{
	return MSXDevice::unmappedRead.data();
}
uint8_t* Y8950Periphery::getWriteCacheLine(uint16_t /*address*/)
{
	return MSXDevice::unmappedWrite.data();
}


// MusicModulePeriphery implementation:

MusicModulePeriphery::MusicModulePeriphery(MSXAudio& audio_)
	: audio(audio_)
{
}

void MusicModulePeriphery::write(uint4_t outputs, uint4_t values,
                                 EmuTime::param time)
{
	uint4_t actual = (outputs & values) | (~outputs & read(time));
	audio.y8950.setEnabled((actual & 8) != 0, time);
	audio.enableDAC((actual & 1) != 0, time);
}

uint4_t MusicModulePeriphery::read(EmuTime::param /*time*/)
{
	// IO2-IO1 are unconnected, reading them initially returns the last
	// written value, but after some seconds it falls back to '0'
	// IO3 and IO0 are output pins, but reading them return respectively
	// '1' and '0'
	return 8;
}


// PanasonicAudioPeriphery implementation:

PanasonicAudioPeriphery::PanasonicAudioPeriphery(
		MSXAudio& audio_, DeviceConfig& config,
		const std::string& soundDeviceName)
	: audio(audio_)
	, swSwitch(audio.getCommandController(), tmpStrCat(soundDeviceName, "_firmware"),
	           "This setting controls the switch on the Panasonic "
	           "MSX-AUDIO module. The switch controls whether the internal "
	           "software of this module must be started or not.",
	           false)
	// note: name + " RAM"  already taken by sample RAM
	, ram(config, audio.getName() + " mapped RAM",
	      "MSX-AUDIO mapped RAM", 0x1000)
	, rom(audio.getName() + " ROM", "MSX-AUDIO ROM", config)
{
	reset();
}

PanasonicAudioPeriphery::~PanasonicAudioPeriphery()
{
	setIOPorts(0); // unregister IO ports
}

void PanasonicAudioPeriphery::reset()
{
	ram.clear(); // TODO check
	setBank(0);
	setIOPorts(0); // TODO check: neither IO port ranges active
}

void PanasonicAudioPeriphery::write(uint4_t outputs, uint4_t values,
                                    EmuTime::param time)
{
	uint4_t actual = (outputs & values) | (~outputs & read(time));
	audio.y8950.setEnabled(!(actual & 8), time);
}

uint4_t PanasonicAudioPeriphery::read(EmuTime::param /*time*/)
{
	// verified bit 0,1,3 read as zero
	return swSwitch.getBoolean() ? 0x4 : 0x0; // bit2
}

uint8_t PanasonicAudioPeriphery::peekMem(uint16_t address, EmuTime::param /*time*/) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return ram[(address & 0x3FFF) - 0x3000];
	} else {
		return rom[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

const uint8_t* PanasonicAudioPeriphery::getReadCacheLine(uint16_t address) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return &ram[(address & 0x3FFF) - 0x3000];
	} else {
		return &rom[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

void PanasonicAudioPeriphery::writeMem(uint16_t address, uint8_t value, EmuTime::param /*time*/)
{
	address &= 0x7FFF;
	if (address == 0x7FFE) {
		setBank(value);
	} else if (address == 0x7FFF) {
		setIOPorts(value);
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		ram[address - 0x3000] = value;
	}
}

uint8_t* PanasonicAudioPeriphery::getWriteCacheLine(uint16_t address)
{
	address &= 0x7FFF;
	if (address == (0x7FFE & CacheLine::HIGH)) {
		return nullptr;
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		return &ram[address - 0x3000];
	} else {
		return MSXDevice::unmappedWrite.data();
	}
}

void PanasonicAudioPeriphery::setBank(uint8_t value)
{
	bankSelect = value & 3;
	audio.getCPU().invalidateAllSlotsRWCache(0x0000, 0x10000);
}

void PanasonicAudioPeriphery::setIOPorts(uint8_t value)
{
	uint8_t diff = ioPorts ^ value;
	if (diff & 1) {
		setIOPortsHelper(0xC0, (value & 1) != 0);
	}
	if (diff & 2) {
		setIOPortsHelper(0xC2, (value & 2) != 0);
	}
	ioPorts = value;
}
void PanasonicAudioPeriphery::setIOPortsHelper(uint8_t base, bool enable)
{
	MSXCPUInterface& cpu = audio.getCPUInterface();
	if (enable) {
		cpu.register_IO_InOut_range(base, 2, &audio);
	} else {
		cpu.unregister_IO_InOut_range(base, 2, &audio);
	}
}

template<typename Archive>
void PanasonicAudioPeriphery::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ram",        ram,
	             "bankSelect", bankSelect);
	uint8_t tmpIoPorts = ioPorts;
	ar.serialize("ioPorts", tmpIoPorts);
	if constexpr (Archive::IS_LOADER) {
		setIOPorts(tmpIoPorts);
	}
}


// ToshibaAudioPeriphery implementation:

ToshibaAudioPeriphery::ToshibaAudioPeriphery(MSXAudio& audio_)
	: audio(audio_)
{
}

void ToshibaAudioPeriphery::write(uint4_t /*outputs*/, uint4_t /*values*/,
                                  EmuTime::param /*time*/)
{
	// TODO IO1-IO0 are programmed as output by HX-MU900 software rom
	//      and it writes periodically the values 1/1/2/2/0/0 to
	//      these pins, but I have no idea what function they have
}

uint4_t ToshibaAudioPeriphery::read(EmuTime::param /*time*/)
{
	// IO3-IO2 are unconnected (see also comment in MusicModulePeriphery)
	// IO1-IO0 are output pins, but reading them returns '1'
	return 0x3;
}

void ToshibaAudioPeriphery::setSPOFF(bool value, EmuTime::param time)
{
	audio.y8950.setEnabled(!value, time);
}


// Y8950PeripheryFactory implementation:

std::unique_ptr<Y8950Periphery> Y8950PeripheryFactory::create(
	MSXAudio& audio, DeviceConfig& config,
	const std::string& soundDeviceName)
{
	auto type = config.getChildData("type", "philips");
	StringOp::casecmp cmp;
	if (cmp(type, "philips")) {
		return std::make_unique<MusicModulePeriphery>(audio);
	} else if (cmp(type, "panasonic")) {
		return std::make_unique<PanasonicAudioPeriphery>(
			audio, config, soundDeviceName);
	} else if (cmp(type, "toshiba")) {
		return std::make_unique<ToshibaAudioPeriphery>(audio);
	}
	throw MSXException("Unknown MSX-AUDIO type: ", type);
}

} // namespace openmsx
