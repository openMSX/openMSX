// $Id: omSynthesizer.cc 4531 2005-06-26 19:17:47Z m9710797 $

#include "RomPlayBall.hh"
#include "SamplePlayer.hh"
#include "CacheLine.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomPlayBall::RomPlayBall(MSXMotherBoard& motherBoard, const XMLElement& config,
                         std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, rom)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);

	samplePlayer.reset(new SamplePlayer(motherBoard, "Playball-DAC",
	                         "Sony Playball's DAC", config,
	                         "playball/playball_", 15));
	reset(*static_cast<EmuTime*>(0));
}

RomPlayBall::~RomPlayBall()
{
}

void RomPlayBall::reset(const EmuTime& /*time*/)
{
	samplePlayer->reset();
}

byte RomPlayBall::peekMem(word address, const EmuTime& time) const
{
	if (address == 0xBFFF) {
		return samplePlayer->isPlaying() ? 0xFE : 0xFF;
	} else {
		return Rom16kBBlocks::peekMem(address, time);
	}
}

byte RomPlayBall::readMem(word address, const EmuTime& time)
{
	return peekMem(address, time);
}

const byte* RomPlayBall::getReadCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return NULL;
	} else {
		return Rom16kBBlocks::getReadCacheLine(address);
	}
}

void RomPlayBall::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if (address == 0xBFFF) {
		if ((value <= 14) && !samplePlayer->isPlaying()) {
			samplePlayer->play(value);
		}
	}
}

byte* RomPlayBall::getWriteCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0xBFFF & CacheLine::HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void RomPlayBall::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Rom16kBBlocks>(*this);
	ar.serialize("SamplePlayer", *samplePlayer);
}
INSTANTIATE_SERIALIZE_METHODS(RomPlayBall);

} // namespace openmsx
