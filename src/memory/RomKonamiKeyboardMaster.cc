// $Id$

#include "RomKonamiKeyboardMaster.hh"
#include "VLM5030.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

RomKonamiKeyboardMaster::RomKonamiKeyboardMaster(
		MSXMotherBoard& motherBoard, const DeviceConfig& config,
		std::auto_ptr<Rom> rom_)
	: Rom16kBBlocks(motherBoard, config, rom_)
	, vlm5030(new VLM5030(motherBoard, "VLM5030",
	                      "Konami Keyboard Master's VLM5030",
	                      rom->getFilename(), config))
{
	setUnmapped(0);
	setRom(1, 0);
	setUnmapped(2);
	setUnmapped(3);

	reset(EmuTime::dummy());

	getMotherBoard().getCPUInterface().register_IO_Out(0x00, this);
	getMotherBoard().getCPUInterface().register_IO_Out(0x20, this);
	getMotherBoard().getCPUInterface().register_IO_In(0x00, this);
	getMotherBoard().getCPUInterface().register_IO_In(0x20, this);
}

RomKonamiKeyboardMaster::~RomKonamiKeyboardMaster()
{
	getMotherBoard().getCPUInterface().unregister_IO_Out(0x00, this);
	getMotherBoard().getCPUInterface().unregister_IO_Out(0x20, this);
	getMotherBoard().getCPUInterface().unregister_IO_In(0x00, this);
	getMotherBoard().getCPUInterface().unregister_IO_In(0x20, this);
}

void RomKonamiKeyboardMaster::reset(EmuTime::param /*time*/)
{
	vlm5030->reset();
}

void RomKonamiKeyboardMaster::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0xFF) {
	case 0x00:
		vlm5030->writeData(value);
		break;
	case 0x20:
		vlm5030->writeControl(value, time);
		break;
	default:
		UNREACHABLE;
	}
}

byte RomKonamiKeyboardMaster::readIO(word port, EmuTime::param time)
{
	return RomKonamiKeyboardMaster::peekIO(port, time);
}

byte RomKonamiKeyboardMaster::peekIO(word port, EmuTime::param time) const
{
	switch (port & 0xFF) {
	case 0x00:
		return vlm5030->getBSY(time) ? 0x10 : 0x00;
	case 0x20:
		return 0xFF;
	default:
		UNREACHABLE; return 0xFF;
	}
}

template<typename Archive>
void RomKonamiKeyboardMaster::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("VLM5030", *vlm5030);
}
INSTANTIATE_SERIALIZE_METHODS(RomKonamiKeyboardMaster);
REGISTER_MSXDEVICE(RomKonamiKeyboardMaster, "RomKonamiKeyboardMaster");

} // namespace openmsx
