#include "MSXYamahaSFG.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

static YM2151::Variant parseVariant(const DeviceConfig& config)
{
	auto variant = config.getChildData("variant", "YM2151");
	if (variant == "YM2151") return YM2151::Variant::YM2151;
	if (variant == "YM2164") return YM2151::Variant::YM2164;
	throw MSXException("Invalid variant '", variant, "', expected 'YM2151' or 'YM2164'.");
}

MSXYamahaSFG::MSXYamahaSFG(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, ym2151(getName(), "Yamaha SFG-01/05", config, getCurrentTime(), parseVariant(config))
	, ym2148(getName(), getMotherBoard())
{
	reset(getCurrentTime());
}

void MSXYamahaSFG::reset(EmuTime::param time)
{
	ym2151.reset(time);
	ym2148.reset();
	registerLatch = 0; // TODO check
	irqVector = 255; // TODO check
	irqVector2148 = 255; // TODO check
}

void MSXYamahaSFG::writeMem(word address, byte value, EmuTime::param time)
{
	word maskedAddress = address & 0x3FFF;
	switch (maskedAddress) {
	case 0x3FF0: // OPM ADDRESS REGISTER
		writeRegisterPort(value, time);
		break;
	case 0x3FF1: // OPM DATA REGISTER
		writeDataPort(value, time);
		break;
	case 0x3FF2: // Register for data latched to ST0 to ST7 output ports
		// TODO: keyboardLatch = value;
		//std::cerr << "TODO: keyboardLatch = " << (int)value << '\n';
		break;
	case 0x3FF3: // MIDI IRQ VECTOR ADDRESS REGISTER
		irqVector2148 = value;
		break;
	case 0x3FF4: // EXTERNAL IRQ VECTOR ADDRESS REGISTER
		// IRQ vector for YM2151 (+ default vector ???)
		irqVector = value;
		break;
	case 0x3FF5: // MIDI standard UART DATA WRITE BUFFER
		ym2148.writeData(value, time);
		break;
	case 0x3FF6: // MIDI standard UART COMMAND REGISTER
		ym2148.writeCommand(value);
		break;
	}
}

byte* MSXYamahaSFG::getWriteCacheLine(word start) const
{
	if ((start & 0x3FFF & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	}
	return unmappedWrite;
}

byte MSXYamahaSFG::readIRQVector()
{
	return ym2148.pendingIRQ() ? irqVector2148 : irqVector;
}

void MSXYamahaSFG::writeRegisterPort(byte value, EmuTime::param /*time*/)
{
	registerLatch = value;
}

void MSXYamahaSFG::writeDataPort(byte value, EmuTime::param time)
{
	ym2151.writeReg(registerLatch, value, time);
}

byte MSXYamahaSFG::readMem(word address, EmuTime::param time)
{
	word maskedAddress = address & 0x3FFF;
	if (maskedAddress < 0x3FF0 || maskedAddress >= 0x3FF8) {
		return peekMem(address, time);
	}
	switch (maskedAddress) {
	case 0x3FF0: // (not used, it seems)
	case 0x3FF1: // OPM STATUS REGISTER
	case 0x3FF2: // Data buffer for SD0 to SD7 input ports
		return peekMem(address, time);
	case 0x3FF5: // MIDI standard UART DATA READ BUFFER
		return ym2148.readData(time);
	case 0x3FF6: // MIDI standard UART STATUS REGISTER
		return ym2148.readStatus(time);
	}
	return 0xFF;
}

byte MSXYamahaSFG::peekMem(word address, EmuTime::param time) const
{
	word maskedAddress = address & 0x3FFF;
	if (maskedAddress < 0x3FF0 || maskedAddress >= 0x3FF8) {
		// size can also be 16kB for SFG-01 or 32kB for SFG-05
		return rom[address & (rom.getSize() - 1)];
	}
	switch (maskedAddress) {
	case 0x3FF0: // (not used, it seems)
		return 0xFF;
	case 0x3FF1: // OPM STATUS REGISTER
		return ym2151.readStatus();
	case 0x3FF2: // Data buffer for SD0 to SD7 input ports
		// TODO: return getKbdStatus();
		break;
	case 0x3FF5: // MIDI standard UART DATA READ BUFFER
		return ym2148.peekData(time);
	case 0x3FF6: // MIDI standard UART STATUS REGISTER
		return ym2148.peekStatus(time);
	}
	return 0xFF;
}

const byte* MSXYamahaSFG::getReadCacheLine(word start) const
{
	if ((start & 0x3FFF & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	}
	return &rom[start & (rom.getSize() - 1)];
}

// version 1: initial version
// version 2: moved irqVector2148 from ym2148 to here
template<typename Archive>
void MSXYamahaSFG::serialize(Archive& ar, unsigned version)
{
	ar.serialize("YM2151",        ym2151,
	             "YM2148",        ym2148,
	             "registerLatch", registerLatch,
	             "irqVector",     irqVector);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("irqVector2148", irqVector2148);
	} else {
		irqVector2148 = 255; // could be retrieved from the old ym2148
		                     // savestate data, but I didn't bother
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXYamahaSFG);
REGISTER_MSXDEVICE(MSXYamahaSFG, "YamahaSFG");

} // namespace openmsx
