// $Id: omSynthesizer.cc 4531 2005-06-26 19:17:47Z m9710797 $

#include "RomPlayBall.hh"
#include "MSXMotherBoard.hh"
#include "SamplePlayer.hh"
#include "PlayBallSamples.hh"
#include "CPU.hh"
#include "Rom.hh"

namespace openmsx {

RomPlayBall::RomPlayBall(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(motherBoard, config, time, rom)
{
	setBank(0, unmappedRead);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmappedRead);

	samplePlayer.reset(new SamplePlayer(motherBoard.getMixer(), getName(),
	                         "Sony PlayBall DAC", config));

	reset(time);
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
	if ((address & CPU::CACHE_LINE_HIGH) == (0xBFFF & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return Rom16kBBlocks::getReadCacheLine(address);
	}
}

void RomPlayBall::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	const byte* sampleBuf[15] = {
		playball0,  playball1,  playball2,  playball3,
		playball4,  playball0,  playball6,  playball7,
		playball8,  playball9,  playball10, playball11,
		playball12, playball13, playball14
	};
	unsigned sampleSize[15] = {
		sizeof(playball0),  sizeof(playball1),  sizeof(playball2),
		sizeof(playball3),  sizeof(playball4),  sizeof(playball0),
		sizeof(playball6),  sizeof(playball7),  sizeof(playball8),
		sizeof(playball9),  sizeof(playball10), sizeof(playball11),
		sizeof(playball12), sizeof(playball13), sizeof(playball14)
	};

	if (address == 0xBFFF) {
		if ((value <= 14) && !samplePlayer->isPlaying()) {
			samplePlayer->play(sampleBuf[value], sampleSize[value],
			                   8, 11025);
		}
	}
}

byte* RomPlayBall::getWriteCacheLine(word address) const
{
	if ((address & CPU::CACHE_LINE_HIGH) == (0xBFFF & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
