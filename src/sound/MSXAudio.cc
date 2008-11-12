// $Id$

#include "MSXAudio.hh"
#include "Y8950.hh"
#include "Y8950Periphery.hh"
#include "DACSound8U.hh"
#include "Ram.hh"
#include "Rom.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "CacheLine.hh"
#include "MSXMotherBoard.hh"
#include "BooleanSetting.hh"
#include "CommandController.hh"
#include "StringOp.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "serialize.hh"

using std::string;

namespace openmsx {

class MusicModulePeriphery : public Y8950Periphery
{
public:
	explicit MusicModulePeriphery(MSXAudio& audio);
	virtual void write(nibble outputs, nibble values, EmuTime::param time);
	virtual nibble read(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& /*ar*/, unsigned /*version*/) {
		// nothing
	}

private:
	MSXAudio& audio;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, MusicModulePeriphery, "MusicModule");

class PanasonicAudioPeriphery : public Y8950Periphery
{
public:
	PanasonicAudioPeriphery(MSXAudio& audio, const XMLElement& config);
	~PanasonicAudioPeriphery();

	virtual void reset();

	virtual void write(nibble outputs, nibble values, EmuTime::param time);
	virtual nibble read(EmuTime::param time);

	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte value);
	void setIOPorts(byte value);
	void setIOPortsHelper(unsigned base, bool enable);

	MSXAudio& audio;
	BooleanSetting swSwitch;
	const std::auto_ptr<Ram> ram;
	const std::auto_ptr<Rom> rom;
	byte bankSelect;
	byte ioPorts;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, PanasonicAudioPeriphery, "Panasonic");

class ToshibaAudioPeriphery : public Y8950Periphery
{
public:
	explicit ToshibaAudioPeriphery(MSXAudio& audio);
	virtual void write(nibble outputs, nibble values, EmuTime::param time);
	virtual nibble read(EmuTime::param time);
	virtual void setSPOFF(bool value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& /*ar*/, unsigned /*version*/) {
		// nothing
	}

private:
	MSXAudio& audio;
};
REGISTER_POLYMORPHIC_INITIALIZER(Y8950Periphery, ToshibaAudioPeriphery, "Toshiba");


// MSXAudio

MSXAudio::MSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, dacValue(0x80), dacEnabled(false)
{
	string type = StringOp::toLower(config.getChildData("type", "philips"));
	if (type == "philips") {
		periphery.reset(new MusicModulePeriphery(*this));
		dac.reset(new DACSound8U(motherBoard.getMSXMixer(),
		               getName() + " 8-bit DAC", "MSX-AUDIO 8-bit DAC",
		               config));
	} else if (type == "panasonic") {
		periphery.reset(new PanasonicAudioPeriphery(*this, config));
	} else if (type == "toshiba") {
		periphery.reset(new ToshibaAudioPeriphery(*this));
	} else {
		throw MSXException("Unknown MSX-AUDIO type: " + type);
	}
	int ramSize = config.getChildDataAsInt("sampleram", 256); // size in kb
	EmuTime::param time = getCurrentTime();
	y8950.reset(new Y8950(motherBoard, getName(), config, ramSize * 1024,
	                      time, *periphery));
	reset(time);
}

MSXAudio::~MSXAudio()
{
	// delete soon, because PanasonicAudioPeriphery still uses
	// this object in its destructor
	periphery.reset();
}

void MSXAudio::reset(EmuTime::param time)
{
	y8950->reset(time);
	periphery->reset();
	registerLatch = 0; // TODO check
}

byte MSXAudio::readIO(word port, EmuTime::param time)
{
	byte result;
	if ((port & 0xFF) == 0x0A) {
		// read DAC
		result = 255;
	} else {
		result = (port & 1) ? y8950->readReg(registerLatch, time)
		                    : y8950->readStatus();
	}
	//std::cout << "read:  " << (int)(port& 0xff) << " " << (int)result << std::endl;
	return result;
}

byte MSXAudio::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xFF) == 0x0A) {
		// read DAC
		return 255; // read always returns 255
	} else {
		return (port & 1) ? y8950->peekReg(registerLatch, time)
		                  : y8950->peekStatus();
	}
}

void MSXAudio::writeIO(word port, byte value, EmuTime::param time)
{
	//std::cout << "write: " << (int)(port& 0xff) << " " << (int)value << std::endl;
	if ((port & 0xFF) == 0x0A) {
		dacValue = value;
		if (dacEnabled) {
			assert(dac.get());
			dac->writeDAC(dacValue, time);
		}
	} else if ((port & 0x01) == 0) {
		// 0xC0 or 0xC2
		registerLatch = value;
	} else {
		// 0xC1 or 0xC3
		y8950->writeReg(registerLatch, value, time);
	}
}

byte MSXAudio::readMem(word address, EmuTime::param time)
{
	return periphery->readMem(address, time);
}
byte MSXAudio::peekMem(word address, EmuTime::param time) const
{
	return periphery->peekMem(address, time);
}
void MSXAudio::writeMem(word address, byte value, EmuTime::param time)
{
	periphery->writeMem(address, value, time);
}
const byte* MSXAudio::getReadCacheLine(word start) const
{
	return periphery->getReadCacheLine(start);
}
byte* MSXAudio::getWriteCacheLine(word start) const
{
	return periphery->getWriteCacheLine(start);
}

void MSXAudio::enableDAC(bool enable, EmuTime::param time)
{
	if ((dacEnabled != enable) && dac.get()) {
		dacEnabled = enable;
		byte value = dacEnabled ? dacValue : 0x80;
		dac->writeDAC(value, time);
	}
}


// MusicModulePeriphery

MusicModulePeriphery::MusicModulePeriphery(MSXAudio& audio_)
	: audio(audio_)
{
}

void MusicModulePeriphery::write(nibble outputs, nibble values,
                                 EmuTime::param time)
{
	nibble actual = (outputs & values) | (~outputs & read(time));
	audio.y8950->setEnabled(actual & 8, time);
	audio.enableDAC(actual & 1, time);
}

nibble MusicModulePeriphery::read(EmuTime::param /*time*/)
{
	// IO2-IO1 are unconnected, reading them initially returns the last
	// written value, but after some seconds it falls back to '0'
	// IO3 and IO0 are output pins, but reading them return respectively
	// '1' and '0'
	return 8;
}


// PanasonicAudioPeriphery

static string generateName(CommandController& controller)
{
	// TODO better name?
	return controller.makeUniqueSettingName("PanasonicAudioSwitch");
}

PanasonicAudioPeriphery::PanasonicAudioPeriphery(
		MSXAudio& audio_, const XMLElement& config)
	: audio(audio_)
	, swSwitch(audio.getMotherBoard().getCommandController(),
	           generateName(audio.getMotherBoard().getCommandController()),
	           "This setting controls the switch on the Panasonic "
	           "MSX-AUDIO module. The switch controls whether the internal "
	           "software of this module must be started or not.",
	           false)
	// note: name + " RAM"  already taken by sample RAM
	, ram(new Ram(audio.getMotherBoard(), audio.getName() + " mapped RAM",
	              "MSX-AUDIO mapped RAM", 0x1000))
	, rom(new Rom(audio.getMotherBoard(), audio.getName() + " ROM",
	              "MSX-AUDIO ROM", config))
	, ioPorts(0)
{
	reset();
}

PanasonicAudioPeriphery::~PanasonicAudioPeriphery()
{
	setIOPorts(0); // unregister IO ports
}

void PanasonicAudioPeriphery::reset()
{
	ram->clear(); // TODO check
	setBank(0);
	setIOPorts(0); // TODO check: neither IO port ranges active
}

void PanasonicAudioPeriphery::write(nibble outputs, nibble values,
                                    EmuTime::param time)
{
	nibble actual = (outputs & values) | (~outputs & read(time));
	audio.y8950->setEnabled(!(actual & 8), time);
}

nibble PanasonicAudioPeriphery::read(EmuTime::param /*time*/)
{
	// verified bit 0,1,3 read as zero
	return swSwitch.getValue() ? 0x4 : 0x0; // bit2
}

byte PanasonicAudioPeriphery::peekMem(word address, EmuTime::param /*time*/) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return (*ram)[(address & 0x3FFF) - 0x3000];
	} else {
		return (*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

const byte* PanasonicAudioPeriphery::getReadCacheLine(word address) const
{
	if ((bankSelect == 0) && ((address & 0x3FFF) >= 0x3000)) {
		return &(*ram)[(address & 0x3FFF) - 0x3000];
	} else {
		return &(*rom)[0x8000 * bankSelect + (address & 0x7FFF)];
	}
}

void PanasonicAudioPeriphery::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	address &= 0x7FFF;
	if (address == 0x7FFE) {
		setBank(value);
	} else if (address == 0x7FFF) {
		setIOPorts(value);
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		(*ram)[address - 0x3000] = value;
	}
}

byte* PanasonicAudioPeriphery::getWriteCacheLine(word address) const
{
	address &= 0x7FFF;
	if (address == (0x7FFE & CacheLine::HIGH)) {
		return NULL;
	}
	address &= 0x3FFF;
	if ((bankSelect == 0) && (address >= 0x3000)) {
		return const_cast<byte*>(&(*ram)[address - 0x3000]);
	} else {
		return MSXDevice::unmappedWrite;
	}
}

void PanasonicAudioPeriphery::setBank(byte value)
{
	bankSelect = value & 3;
	audio.getMotherBoard().getCPU().invalidateMemCache(0x0000, 0x10000);
}

void PanasonicAudioPeriphery::setIOPorts(byte value)
{
	byte diff = ioPorts ^ value;
	if (diff & 1) {
		setIOPortsHelper(0xC0, value & 1);
	}
	if (diff & 2) {
		setIOPortsHelper(0xC2, value & 2);
	}
	ioPorts = value;
}
void PanasonicAudioPeriphery::setIOPortsHelper(unsigned base, bool enable)
{
	MSXCPUInterface& cpu = audio.getMotherBoard().getCPUInterface();
	if (enable) {
		cpu.register_IO_In (base + 0, &audio);
		cpu.register_IO_In (base + 1, &audio);
		cpu.register_IO_Out(base + 0, &audio);
		cpu.register_IO_Out(base + 1, &audio);
	} else {
		cpu.unregister_IO_In (base + 0, &audio);
		cpu.unregister_IO_In (base + 1, &audio);
		cpu.unregister_IO_Out(base + 0, &audio);
		cpu.unregister_IO_Out(base + 1, &audio);
	}
}


// ToshibaAudioPeriphery

ToshibaAudioPeriphery::ToshibaAudioPeriphery(MSXAudio& audio_)
	: audio(audio_)
{
}

void ToshibaAudioPeriphery::write(nibble /*outputs*/, nibble /*values*/,
                                    EmuTime::param /*time*/)
{
	// TODO IO1-IO0 are programmed as output by HX-MU900 software rom
	//      and it writes periodically the values 1/1/2/2/0/0 to
	//      these pins, but I have no idea what function they have
}

nibble ToshibaAudioPeriphery::read(EmuTime::param /*time*/)
{
	// IO3-IO2 are unconnected (see also comment in MusicModulePeriphery)
	// IO1-IO0 are output pins, but reading them returns '1'
	return 0x3;
}

void ToshibaAudioPeriphery::setSPOFF(bool value, EmuTime::param time)
{
	audio.y8950->setEnabled(!value, time);
}


template<typename Archive>
void PanasonicAudioPeriphery::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ram", *ram);
	ar.serialize("bankSelect", bankSelect);
	byte tmpIoPorts = ioPorts;
	ar.serialize("ioPorts", tmpIoPorts);
	if (ar.isLoader()) {
		setIOPorts(tmpIoPorts);
	}
}

template<typename Archive>
void MSXAudio::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serializePolymorphic("periphery", *periphery);
	ar.serialize("Y8950", *y8950);

	ar.serialize("registerLatch", registerLatch);
	ar.serialize("dacValue", dacValue);
	ar.serialize("dacEnabled", dacEnabled);

	if (ar.isLoader()) {
		// restore dac status
		if (dacEnabled) {
			assert(dac.get());
			dac->writeDAC(dacValue, getCurrentTime());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXAudio);
REGISTER_MSXDEVICE(MSXAudio, "MSX-Audio");

} // namespace openmsx
