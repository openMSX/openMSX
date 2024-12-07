#include "RomKonamiKeyboardMaster.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

RomKonamiKeyboardMaster::RomKonamiKeyboardMaster(
		const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
	, vlm5030("VLM5030", "Konami Keyboard Master's VLM5030",
	          rom.getFilename(), config)
{
	setUnmapped(0);
	setRom(1, 0);
	setUnmapped(2);
	setUnmapped(3);

	reset(EmuTime::dummy());

	auto& cpuInterface = getCPUInterface();
	cpuInterface.register_IO_InOut(0x00, this);
	cpuInterface.register_IO_InOut(0x20, this);
}

RomKonamiKeyboardMaster::~RomKonamiKeyboardMaster()
{
	auto& cpuInterface = getCPUInterface();
	cpuInterface.unregister_IO_InOut(0x00, this);
	cpuInterface.unregister_IO_InOut(0x20, this);
}

void RomKonamiKeyboardMaster::reset(EmuTime::param /*time*/)
{
	vlm5030.reset();
}

void RomKonamiKeyboardMaster::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0xFF) {
	case 0x00:
		vlm5030.writeData(value);
		break;
	case 0x20:
		vlm5030.writeControl(value, time);
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
		return vlm5030.getBSY(time) ? 0x10 : 0x00;
	case 0x20:
		return 0xFF;
	default:
		UNREACHABLE;
	}
}

template<typename Archive>
void RomKonamiKeyboardMaster::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("VLM5030", vlm5030);
}
INSTANTIATE_SERIALIZE_METHODS(RomKonamiKeyboardMaster);
REGISTER_MSXDEVICE(RomKonamiKeyboardMaster, "RomKonamiKeyboardMaster");

} // namespace openmsx
