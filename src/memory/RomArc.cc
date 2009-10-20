// $Id$

// Parallax' Arc

#include "RomArc.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomArc::RomArc(MSXMotherBoard& motherBoard, const XMLElement& config,
	       std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 1);
	setUnmapped(3);

	reset(EmuTime::dummy());

	getMotherBoard().getCPUInterface().register_IO_Out(0x7f, this);
	getMotherBoard().getCPUInterface().register_IO_In (0x7f, this);
}

RomArc::~RomArc()
{
	getMotherBoard().getCPUInterface().unregister_IO_Out(0x7f, this);
	getMotherBoard().getCPUInterface().unregister_IO_In (0x7f, this);
}

void RomArc::reset(EmuTime::param /*time*/)
{
	offset = 0x00;
}

void RomArc::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	if (value == 0x35) {
		++offset;
	}
}

byte RomArc::readIO(word port, EmuTime::param time)
{
	return RomArc::peekIO(port, time);
}

byte RomArc::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return ((offset & 0x03) == 0x03) ? 0xda : 0xff;
}

template<typename Archive>
void RomArc::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("offset", offset);
}
INSTANTIATE_SERIALIZE_METHODS(RomArc);
REGISTER_MSXDEVICE(RomArc, "RomArc");

} // namespace openmsx
