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

// forward declarations
class EmuTime;


#define MIN(x,y)	(x) < (y) ? (x) : (y)


class AY8910Interface
{
	public:
		virtual byte readA(const EmuTime &time)=0;
		virtual byte readB(const EmuTime &time)=0;
		virtual void writeA(byte value, const EmuTime &time)=0;
		virtual void writeB(byte value, const EmuTime &time)=0;
};

class AY8910 : public SoundDevice
{
	public:
		AY8910(AY8910Interface &interf, short volume, const EmuTime &time); 
		virtual ~AY8910(); 
	
		byte readRegister(byte reg, const EmuTime &time);
		void writeRegister(byte reg, byte value, const EmuTime &time);

		//SoundDevice
		void reset(const EmuTime &time);
		void setInternalVolume(short newVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);
		
	private:
		void wrtReg(byte reg, byte value, const EmuTime &time);
		void checkMute();
		
		static const int FP_UNIT = 0x8000;	// fixed point representation of 1
		static const int CLOCK = 3579545/2;	// real time clock frequency of AY8910

		static const int AY_AFINE   = 0;
		static const int AY_ACOARSE = 1;
		static const int AY_BFINE   = 2;
		static const int AY_BCOARSE = 3;
		static const int AY_CFINE   = 4;
		static const int AY_CCOARSE = 5;
		static const int AY_NOISEPER= 6;
		static const int AY_ENABLE  = 7;
		static const int PORT_A_DIRECTION = 0x40;
		static const int PORT_B_DIRECTION = 0x80;
		static const int AY_AVOL    = 8;
		static const int AY_BVOL    = 9;
		static const int AY_CVOL    = 10;
		static const int AY_EFINE   = 11;
		static const int AY_ECOARSE = 12;
		static const int AY_ESHAPE  = 13;
		static const int AY_PORTA   = 14;
		static const int AY_PORTB   = 15;

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
		
		AY8910Interface &interface;
};
#endif
