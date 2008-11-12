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

class Y8950Adpcm : public Schedulable
{
public:
	Y8950Adpcm(Y8950& y8950, MSXMotherBoard& motherBoard,
	           const std::string& name, unsigned sampleRam);
	virtual ~Y8950Adpcm();

	void reset(EmuTime::param time);
	bool muted() const;
	void writeReg(byte rg, byte data, EmuTime::param time);
	byte readReg(byte rg);
	byte peekReg(byte rg) const;
	int calcSample();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	void schedule(EmuTime::param time);
	void restart();

	bool playing() const;
	void writeData(byte data);
	byte readData();
	byte peekData() const;
	void writeMemory(byte value);
	byte readMemory() const;

	Y8950& y8950;
	const std::auto_ptr<Ram> ram;

	unsigned startAddr;
	unsigned stopAddr;
	unsigned addrMask;
	unsigned memPntr;

	unsigned nowStep;
	int volume;
	int out, output;
	int diff;
	int nextLeveling;
	int sampleStep;
	int volumeWStep;
	int readDelay;
	int delta;

	byte reg7;
	byte reg15;
	byte adpcm_data;
	bool romBank;
};

} // namespace openmsx

#endif
