#include "DummySCSIDevice.hh"

#include "serialize.hh"

namespace openmsx {

void DummySCSIDevice::reset()
{
	// do nothing
}

bool DummySCSIDevice::isSelected()
{
	return false;
}

unsigned DummySCSIDevice::executeCmd(
	std::span<const uint8_t, 12> /*cdb*/, SCSI::Phase& /*phase*/, unsigned& /*blocks*/)
{
	// do nothing
	return 0;
}

unsigned DummySCSIDevice::executingCmd(SCSI::Phase& /*phase*/, unsigned& /*blocks*/)
{
	return 0;
}

uint8_t DummySCSIDevice::getStatusCode()
{
	return SCSI::ST_CHECK_CONDITION;
}

int DummySCSIDevice::msgOut(uint8_t /*value*/)
{
	return 0; // TODO: check if this is sane, but it doesn't seem to be used anyway
}

uint8_t DummySCSIDevice::msgIn()
{
	return 0; // TODO: check if this is sane, but it doesn't seem to be used anyway
}

void DummySCSIDevice::disconnect()
{
	// do nothing
}

void DummySCSIDevice::busReset()
{
	// do nothing
}

unsigned DummySCSIDevice::dataIn(unsigned& blocks)
{
	blocks = 0;
	return 0;
}

unsigned DummySCSIDevice::dataOut(unsigned& blocks)
{
	blocks = 0;
	return 0;
}

template<typename Archive>
void DummySCSIDevice::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// nothing
}
INSTANTIATE_SERIALIZE_METHODS(DummySCSIDevice);
REGISTER_POLYMORPHIC_INITIALIZER(SCSIDevice, DummySCSIDevice, "DummySCSIDevice");

} // namespace openmsx
