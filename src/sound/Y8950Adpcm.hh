// $Id$

#ifndef __Y8950ADPCM_HH__
#define __Y8950ADPCM_HH__

#include "openmsx.hh"
#include "Schedulable.hh"

namespace openmsx {

// Forward declarartions
class Y8950;


class Y8950Adpcm : private Schedulable
{
public:
	Y8950Adpcm(Y8950* y8950, int sampleRam);
	virtual ~Y8950Adpcm();
	
	void reset(const EmuTime& time);
	void setSampleRate(int sr);
	bool muted();
	void writeReg(byte rg, byte data, const EmuTime& time);
	byte readReg(byte rg);
	int calcSample();

private:
	void schedule(const EmuTime& time);
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	int CLAP(int min, int x, int max);
	void restart();

	Y8950* y8950;

	int sampleRate;
	
	int ramSize;
	int startAddr;
	int stopAddr;
	int playAddr;
	int addrMask;
	int memPntr;
	byte* wave;
	byte* ramBank;
	byte* romBank;
	
	bool playing;
	int volume;
	word delta;
	unsigned int nowStep, step;
	int out, output;
	int diff;
	int nextLeveling;
	int sampleStep;
	int volumeWStep;
	
	byte reg7;
	byte reg15;
};

} // namespace openmsx


#endif 
