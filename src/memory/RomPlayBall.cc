#include "RomPlayBall.hh"
#include "CacheLine.hh"
#include "FileOperations.hh"
#include "serialize.hh"

namespace openmsx {

RomPlayBall::RomPlayBall(const DeviceConfig& config, Rom&& rom_)
	: Rom16kBBlocks(config, std::move(rom_))
	, samplePlayer(
		"Playball-DAC", "Sony Playball's DAC", config,
		strCat(FileOperations::stripExtension(rom.getFilename()), '_'),
		15, "playball/playball_")
{
	setUnmapped(0);
	setRom(1, 0);
	setRom(2, 1);
	invalidateDeviceRCache(0xBFFF & CacheLine::HIGH, CacheLine::SIZE);
	setUnmapped(3);

	reset(EmuTime::dummy());
}

void RomPlayBall::reset(EmuTime::param /*time*/)
{
	samplePlayer.reset();
}

byte RomPlayBall::peekMem(uint16_t address, EmuTime::param time) const
{
	if (address == 0xBFFF) {
		return samplePlayer.isPlaying() ? 0xFE : 0xFF;
	} else {
		return Rom16kBBlocks::peekMem(address, time);
	}
}

byte RomPlayBall::readMem(uint16_t address, EmuTime::param time)
{
	return RomPlayBall::peekMem(address, time);
}

const byte* RomPlayBall::getReadCacheLine(uint16_t address) const
{
	if ((address & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return Rom16kBBlocks::getReadCacheLine(address);
	}
}

void RomPlayBall::writeMem(uint16_t address, byte value, EmuTime::param /*time*/)
{
	if (address == 0xBFFF) {
		if ((value <= 14) && !samplePlayer.isPlaying()) {
			samplePlayer.play(value);
		}
	}
}

byte* RomPlayBall::getWriteCacheLine(uint16_t address)
{
	if ((address & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void RomPlayBall::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("SamplePlayer", samplePlayer);
}
INSTANTIATE_SERIALIZE_METHODS(RomPlayBall);
REGISTER_MSXDEVICE(RomPlayBall, "RomPlayBall");

} // namespace openmsx
