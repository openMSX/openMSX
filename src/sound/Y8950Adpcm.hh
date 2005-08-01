// $Id$

#ifndef Y8950ADPCM_HH
#define Y8950ADPCM_HH

#include "Schedulable.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Y8950;
class MSXMotherBoard;
class Ram;

class Y8950Adpcm : private Schedulable
{
public:
	Y8950Adpcm(Y8950& y8950, MSXMotherBoard& motherBoard,
	           const std::string& name, unsigned sampleRam);
	virtual ~Y8950Adpcm();

	void reset(const EmuTime& time);
	void setSampleRate(int sr);
	bool muted() const;
	void writeReg(byte rg, byte data, const EmuTime& time);
	byte readReg(byte rg);
	int calcSample();

private:
	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	void schedule(const EmuTime& time);
	int CLAP(int min, int x, int max);
	void restart();

	bool playing() const;
	void writeData(byte data);
	byte readData();
	void writeMemory(byte value);
	byte readMemory();

	Y8950& y8950;
	std::auto_ptr<Ram> ram;

	int sampleRate;

	unsigned startAddr;
	unsigned stopAddr;
	unsigned addrMask;
	unsigned memPntr;
	bool romBank;

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
	byte adpcm_data;
	int readDelay;
};

} // namespace openmsx

#endif
