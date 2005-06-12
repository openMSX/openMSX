// $Id$

#ifndef Y8950ADPCM_HH
#define Y8950ADPCM_HH

#include "openmsx.hh"
#include "Schedulable.hh"
#include "Debuggable.hh"

namespace openmsx {

class Y8950;

class Y8950Adpcm : private Schedulable, private Debuggable
{
public:
	Y8950Adpcm(Y8950& y8950, const std::string& name, int sampleRam);
	virtual ~Y8950Adpcm();

	void reset(const EmuTime& time);
	void setSampleRate(int sr);
	bool muted();
	void writeReg(byte rg, byte data, const EmuTime& time);
	byte readReg(byte rg);
	int calcSample();

private:
	// Debuggable
	virtual unsigned getSize() const;
	virtual const std::string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	void schedule(const EmuTime& time);
	int CLAP(int min, int x, int max);
	void restart();

	Y8950& y8950;
	const std::string name;

	int sampleRate;

	int ramSize;
	int startAddr;
	int stopAddr;
	int playAddr;
	int addrMask;
	int memPntr;
	bool romBank;
	byte* ramBank;

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
	int readDelay;
};

} // namespace openmsx

#endif
