// $Id$
// This class implements the AY-3-8910 sound chip
//
// * Only the AY-3-8910 is emulated, no surrounding hardware. 
//   Use the class AY8910Interface to do that.
// * For techncal details on AY-3-8910
//    http://w3.qahwah.net/joost/openMSX/AY-3-8910.pdf

#ifndef __AY8910_HH__
#define __AY8910_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "Debuggable.hh"

namespace openmsx {

class EmuTime;

class AY8910Interface
{
public:
	virtual ~AY8910Interface() {}
	virtual byte readA(const EmuTime& time) = 0;
	virtual byte readB(const EmuTime& time) = 0;
	virtual void writeA(byte value, const EmuTime& time) = 0;
	virtual void writeB(byte value, const EmuTime& time) = 0;
};

class AY8910 : private SoundDevice, private Debuggable
{
public:
	AY8910(AY8910Interface& interf, short volume, const EmuTime& time);
	virtual ~AY8910();

	byte readRegister(byte reg, const EmuTime& time);
	void writeRegister(byte reg, byte value, const EmuTime& time);
	void reset(const EmuTime& time);

private:
	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual int* updateBuffer(int length);

	// Debuggable
	virtual unsigned getSize() const;
	//virtual const string& getDescription() const;  // also in SoundDevice!!
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

	void wrtReg(byte reg, byte value, const EmuTime& time);
	void checkMute();

	byte regs[16];
	unsigned int updateStep;
	int periodA, periodB, periodC, periodN, periodE;
	int countA,  countB,  countC,  countN,  countE;
	unsigned int volA, volB, volC, volE;
	bool envelopeA, envelopeB, envelopeC;
	unsigned char outputA, outputB, outputC, outputN;
	signed char countEnv;
	unsigned char attack;
	bool hold, alternate, holding;
	int random;
	unsigned int volTable[16];
	byte oldEnable;
	
	int* buffer;
	bool semiMuted;
	int validLength;
	
	AY8910Interface& interface;
};

} // namespace openmsx
#endif
