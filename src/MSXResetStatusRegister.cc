#include "MSXResetStatusRegister.hh"
#include "serialize.hh"

namespace openmsx {

MSXResetStatusRegister::MSXResetStatusRegister(const DeviceConfig& config)
	: MSXDevice(config)
	, inverted(config.getChildDataAsBool("inverted", false))
{
	reset(EmuTime::dummy());
}

void MSXResetStatusRegister::reset(EmuTime::param /*time*/)
{
	status = inverted ? 0xFF : 0x00;
}

byte MSXResetStatusRegister::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXResetStatusRegister::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return status;
}

void MSXResetStatusRegister::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	if (inverted) {
		status = value | 0x7F;
	} else {
		status = (status & 0x20) | (value & 0xA0);
	}
}

void MSXResetStatusRegister::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("status", status);
}
INSTANTIATE_SERIALIZE_METHODS(MSXResetStatusRegister);
REGISTER_MSXDEVICE(MSXResetStatusRegister, "F4Device"); // TODO: find a way to handle renames of classes and keep bw compat with savestates....

} // namespace openmsx
