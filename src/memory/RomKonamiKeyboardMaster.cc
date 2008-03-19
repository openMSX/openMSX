// $Id$

#include "RomKonamiKeyboardMaster.hh"
#include "VLM5030.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Rom.hh"
#include <cassert>

namespace openmsx {

RomKonamiKeyboardMaster::RomKonamiKeyboardMaster(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
	, vlm5030(new VLM5030(motherBoard, "VLM5030",
	                      "Konami Keyboard Master's VLM5030", config, time))
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setBank(2, unmappedRead);
	setBank(3, unmappedRead);

	reset(time);

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

void RomKonamiKeyboardMaster::reset(const EmuTime& time)
{
	vlm5030->reset(time);
}

void RomKonamiKeyboardMaster::writeIO(word port, byte value, const EmuTime& time)
{
	switch (port & 0xFF) {
		case 0x00:
			vlm5030->writeData(value);
			break;
		case 0x20:
			vlm5030->writeControl(value, time);
			break;
		default:
			assert(false);
	}
}

byte RomKonamiKeyboardMaster::readIO(word port, const EmuTime& time)
{
	switch (port & 0xFF) {
		case 0x00:
			return vlm5030->getBSY(time) ? 0x10 : 0x00;
			break;
		case 0x20:
			return 0xFF;
			break;
		default:
			assert(false);
			return 0xFF;
	}
}

} // namespace openmsx
