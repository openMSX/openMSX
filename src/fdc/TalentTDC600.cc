// For some documentation see:
//   https://www.msx.org/wiki/Talent_TDC-600
//   https://msxmakers.design.blog/proyectos/proyecto-tdc-600/tdc-600-como-funciona/

#include "TalentTDC600.hh"
#include "serialize.hh"

namespace openmsx {

TalentTDC600::TalentTDC600(const DeviceConfig& config)
	: MSXFDC(config)
	, controller(getScheduler(), reinterpret_cast<DiskDrive**>(drives),
	             getCliComm(), getCurrentTime())
{
	reset(getCurrentTime());
}

void TalentTDC600::reset(EmuTime::param time)
{
	controller.reset(time);
}

byte TalentTDC600::readMem(word address, EmuTime::param time)
{
	if (0x4000 <= address && address < 0x8000) {
		return MSXFDC::readMem(address, time);
	}
	address &= 0x7fff;
	if (address < 0x1000) {
		if (address & 1) {
			return controller.readDataPort(time);
		} else {
			return controller.readStatus(time);
		}
	}
	return 0xff;
}

byte TalentTDC600::peekMem(word address, EmuTime::param time) const
{
	if (0x4000 <= address && address < 0x8000) {
		return MSXFDC::peekMem(address, time);
	}
	address &= 0x7fff;
	if (address < 0x1000) {
		if (address & 1) {
			return controller.peekDataPort(time);
		} else {
			return controller.peekStatus();
		}
	}
	return 0xff;
}

const byte* TalentTDC600::getReadCacheLine(word start) const
{
	if (0x4000 <= start && start < 0x8000) {
		return MSXFDC::getReadCacheLine(start);
	}
	start &= 0x7fff;
	if (start < 0x1000) {
		return nullptr;
	}
	return unmappedRead;
}

void TalentTDC600::writeMem(word address, byte value, EmuTime::param time)
{
	address &= 0x7fff;
	if (address < 0x1000) {
		if (address & 1) {
			controller.writeDataPort(value, time);
		}
	} else if (address < 0x2000) {
		controller.writeControlReg0(value, time);
	}
}

byte* TalentTDC600::getWriteCacheLine(word address) const
{
	address &= 0x7fff;
	if (address < 0x2000) {
		return nullptr;
	}
	return unmappedWrite;
}


template<typename Archive>
void TalentTDC600::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("TC8566AF", controller);
}
INSTANTIATE_SERIALIZE_METHODS(TalentTDC600);
REGISTER_MSXDEVICE(TalentTDC600, "TalentTDC600");

} // namespace openmsx
