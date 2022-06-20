#include "MSXFacMidiInterface.hh"
#include "MidiInDevice.hh"
#include "outer.hh"
#include "serialize.hh"

namespace openmsx {

MSXFacMidiInterface::MSXFacMidiInterface(const DeviceConfig& config)
	: MSXDevice(config)
	, MidiInConnector(MSXDevice::getPluggingController(), MSXDevice::getName() + "-in")
	, outConnector   (MSXDevice::getPluggingController(), MSXDevice::getName() + "-out")
	, i8251(getScheduler(), interf, getCurrentTime())
{
	EmuTime::param time = getCurrentTime();
	static constexpr auto period = EmuDuration::hz(500000); // 500 kHz
	i8251.getClockPin().setPeriodicState(period, period / 2, time);
	reset(time);
}

void MSXFacMidiInterface::reset(EmuTime::param time)
{
	i8251.reset(time);
}

byte MSXFacMidiInterface::readIO(word port, EmuTime::param time)
{
	// 0 -> UART data   register
	// 1 -> UART status register
	return i8251.readIO(port & 1, time);
}

byte MSXFacMidiInterface::peekIO(word port, EmuTime::param time) const
{
	return i8251.peekIO(port & 1, time);
}

void MSXFacMidiInterface::writeIO(word port, byte value, EmuTime::param time)
{
	// 0 -> UART data    register
	// 1 -> UART command register
	i8251.writeIO(port & 1, value, time);
}

// I8251Interface  (pass calls from I8251 to outConnector)

void MSXFacMidiInterface::I8251Interf::setRxRDY(bool /*status*/, EmuTime::param /*time*/)
{
}

void MSXFacMidiInterface::I8251Interf::setDTR(bool /*status*/, EmuTime::param /*time*/)
{
}

void MSXFacMidiInterface::I8251Interf::setRTS(bool /*status*/, EmuTime::param /*time*/)
{
}

bool MSXFacMidiInterface::I8251Interf::getDSR(EmuTime::param /*time*/)
{
	return true;
}

bool MSXFacMidiInterface::I8251Interf::getCTS(EmuTime::param /*time*/)
{
	return true;
}

void MSXFacMidiInterface::I8251Interf::setDataBits(DataBits bits)
{
	auto& midi = OUTER(MSXFacMidiInterface, interf);
	midi.outConnector.setDataBits(bits);
}

void MSXFacMidiInterface::I8251Interf::setStopBits(StopBits bits)
{
	auto& midi = OUTER(MSXFacMidiInterface, interf);
	midi.outConnector.setStopBits(bits);
}

void MSXFacMidiInterface::I8251Interf::setParityBit(bool enable, ParityBit parity)
{
	auto& midi = OUTER(MSXFacMidiInterface, interf);
	midi.outConnector.setParityBit(enable, parity);
}

void MSXFacMidiInterface::I8251Interf::recvByte(byte value, EmuTime::param time)
{
	auto& midi = OUTER(MSXFacMidiInterface, interf);
	midi.outConnector.recvByte(value, time);
}

void MSXFacMidiInterface::I8251Interf::signal(EmuTime::param time)
{
	auto& midi = OUTER(MSXFacMidiInterface, interf);
	midi.getPluggedMidiInDev().signal(time);
}


// MidiInConnector

bool MSXFacMidiInterface::ready()
{
	return i8251.isRecvReady();
}

bool MSXFacMidiInterface::acceptsData()
{
	return i8251.isRecvEnabled();
}

void MSXFacMidiInterface::setDataBits(DataBits bits)
{
	i8251.setDataBits(bits);
}

void MSXFacMidiInterface::setStopBits(StopBits bits)
{
	i8251.setStopBits(bits);
}

void MSXFacMidiInterface::setParityBit(bool enable, ParityBit parity)
{
	i8251.setParityBit(enable, parity);
}

void MSXFacMidiInterface::recvByte(byte value, EmuTime::param time)
{
	i8251.recvByte(value, time);
}


template<typename Archive>
void MSXFacMidiInterface::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<MidiInConnector>(*this);
	ar.serialize("outConnector", outConnector,
	             "I8251",        i8251);
}
INSTANTIATE_SERIALIZE_METHODS(MSXFacMidiInterface);
REGISTER_MSXDEVICE(MSXFacMidiInterface, "MSXFacMidiInterface");

} // namespace openmsx
