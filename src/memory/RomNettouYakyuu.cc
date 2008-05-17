// $Id$

#include "MSXMotherBoard.hh"
#include "RomNettouYakyuu.hh"
#include "Rom.hh"
#include "SamplePlayer.hh"
#include "FileContext.hh"
#include "WavData.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "StringOp.hh"

namespace openmsx {

RomNettouYakyuu::RomNettouYakyuu(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time, std::auto_ptr<Rom> rom)
	: Rom8kBBlocks(motherBoard, config, rom)
{

	samplePlayer.reset(new SamplePlayer(motherBoard, "Nettou Yakyuu-DAC",
	                         "Jaleco Moero!! Nettou Yakuu '88 DAC", config));

	bool alreadyWarned = false;
	for (int i = 0; i < 16; ++i) {
		try {
			SystemFileContext context;
			std::string filename =
				"nettou_yakyuu/nettou_yakyuu_" + StringOp::toString(i) + ".wav";
			sample[i].reset(new WavData(context.resolve(filename)));
		} catch (MSXException& e) {
			if (!alreadyWarned) {
				alreadyWarned = true;
				motherBoard.getMSXCliComm().printWarning(
					"Couldn't read nettou_yakyuu sample data: " +
					e.getMessage() +
					". Continuing without sample data.");
			}
		}
	}

	reset(time);
}

void RomNettouYakyuu::reset(const EmuTime& time)
{
	// ASCII8 behaviour
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);

	samplePlayer->reset();
}

void RomNettouYakyuu::writeMem(word address, byte value, const EmuTime& time)
{
	if ((address < 0x4000) || (address >= 0xC000)) return;

	// mapper stuff, like ASCII8
	if ((0x6000 <= address) && (address < 0x8000)) {
		// calculate region in switch zone
		byte region = ((address >> 11) & 3) + 2;
		redirectToSamplePlayerEnabled[region] = value & 0x80;
		if (redirectToSamplePlayerEnabled[region]) {
			setBank(region, unmappedRead);
		} else {
			setRom(region, value);
		}
		return;
	}

	// sample player stuff
	
	// calculate region in the full address space
	byte region = address >> 13;
	if (!redirectToSamplePlayerEnabled[region]) return; // page not redirected to sample player

//	printf("Write for Sample player...\n");
	
	// bit 7=0: reset
	if (!(value & 0x80)) {
		//printf("Sample player reset...\n");
		samplePlayer->reset();
		return;
	}

/*
 	// TODO: extend the sampleplayer to support looping etc.
	// bit 6=1: no retrigger
	if (value & 0x40) {
		if (!idle) samplePlayerStopAfter(rm->samplePlayer,samplePlayerIsLooping(rm->samplePlayer)!=0);
		return;
	}
	
	if (!idle) {
		if (samplePlayerIsLooping(rm->samplePlayer)) {
			attack_sample=samplePlayerGetLoopBuffer(rm->samplePlayer);
			attack_sample_size=samplePlayerGetLoopBufferSize(rm->samplePlayer);
		}
		else {
			attack_sample=samplePlayerGetAttackBuffer(rm->samplePlayer);
			attack_sample_size=samplePlayerGetAttackBufferSize(rm->samplePlayer);
		}
	}
*/
	// this is wrong, sample player should play sample after it's done with the current one, see the above TODO
	int sampleNr = value & 0xF;
	if (!samplePlayer->isPlaying() && sample[sampleNr].get()) {
		samplePlayer->play(sample[sampleNr]->getData(),
				sample[sampleNr]->getSize(),
				sample[sampleNr]->getBits(),
				sample[sampleNr]->getFreq());
	}
}

byte* RomNettouYakyuu::getWriteCacheLine(word address) const
{
	return NULL;
}

} // namespace openmsx
