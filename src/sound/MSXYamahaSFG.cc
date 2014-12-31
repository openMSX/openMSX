#include "MSXYamahaSFG.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

MSXYamahaSFG::MSXYamahaSFG(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, ym2151(getName(), "Yamaha SFG-01/05", config, getCurrentTime())
{
	reset(getCurrentTime());
}

void MSXYamahaSFG::reset(EmuTime::param time)
{
	ym2151.reset(time);
	ym2148.reset();
	registerLatch = 0; // TODO check
	irqVector = 255; // TODO check
}

void MSXYamahaSFG::writeMem(word address, byte value, EmuTime::param time)
{
	if (address < 0x3FF0 || address >= 0x3FF8) {
		return;
	}

	switch (address & 0x3FFF) {
	case 0x3FF0: // OPM ADDRESS REGISTER
		writeRegisterPort(value, time);
		break;
	case 0x3FF1: // OPM DATA REGISTER
		writeDataPort(value, time);
		break;
	case 0x3FF2: // Register for data latched to ST0 to ST7 output ports
		// TODO: keyboardLatch = value;
		//std::cerr << "TODO: keyboardLatch = " << (int)value << std::endl;
		break;
	case 0x3FF3: // MIDI IRQ VECTOR ADDRESS REGISTER
		ym2148.setVector(value);
		break;
	case 0x3FF4: // EXTERNAL IRQ VECTOR ADDRESS REGISTER
		// IRQ vector for YM2151 (+ default vector ???)
		irqVector = value;
		break;
	case 0x3FF5: // MIDI standard UART DATA READ BUFFER
		ym2148.writeData(value);
		break;
	case 0x3FF6: // MIDI standard UART STATUS REGISTER
		ym2148.writeCommand(value);
		break;
	}
}

byte* MSXYamahaSFG::getWriteCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	}
	return unmappedWrite;
}

byte MSXYamahaSFG::readIRQVector()
{
	return irqVector;
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
	return peekMem(address, time);
}

byte MSXYamahaSFG::peekMem(word address, EmuTime::param /*time*/) const
{
	if (address < 0x3FF0 || address >= 0x3FF8) {
		// size can also be 16kB for SFG-01 or 32kB for SFG-05
		return rom[address & (rom.getSize() - 1)];
	}
	switch (address & 0x3FFF) {
	case 0x3FF0: // OPM STATUS REGISTER
		return ym2151.readStatus();
	case 0x3FF1: // OPM DATA REGISTER
		return ym2151.readStatus();
	case 0x3FF2: // Data buffer for SD0 to SD7 input ports
		// TODO: return getKbdStatus();
		break;
	case 0x3FF5: // MIDI standard UART DATA READ BUFFER
		// TODO create peekData() method
		return const_cast<YM2148&>(ym2148).readData();
		break;
	case 0x3FF6: // MIDI standard UART STATUS REGISTER
		// TODO create peekStatus() method
		return const_cast<YM2148&>(ym2148).readStatus();
		break;
	}
	return 0xFF;
}

const byte* MSXYamahaSFG::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	}
	return &rom[start & (rom.getSize() - 1)];
}

template<typename Archive>
void MSXYamahaSFG::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("YM2151", ym2151);
	ar.serialize("YM2148", ym2148);
	ar.serialize("registerLatch", registerLatch);
	ar.serialize("irqVector", irqVector);
}
INSTANTIATE_SERIALIZE_METHODS(MSXYamahaSFG);
REGISTER_MSXDEVICE(MSXYamahaSFG, "YamahaSFG");

} // namespace openmsx
