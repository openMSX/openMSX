// $Id$

#ifndef __Y8950ADPCM_HH__
#define __Y8950ADPCM_HH__

#include "openmsx.hh"
#include "Schedulable.hh"

// Forward declarartions
class EmuTime;
class Y8950;


class Y8950Adpcm : public Schedulable
{
	public:
		Y8950Adpcm(Y8950 *y8950, int sampleRam);
		virtual ~Y8950Adpcm();
		
		void reset();
		void setSampleRate(int sr);
		bool muted();
		void writeReg(byte rg, byte data, const EmuTime &time);
		byte readReg(byte rg);
		int calcSample();

	private:
		virtual void executeUntilEmuTime(const EmuTime &time, int userData);
		int CLAP(int min, int x, int max);
		void restart();

		Y8950 *y8950;

		int sampleRate;
		
		int ramSize;
		int startAddr;
		int stopAddr;
		int playAddr;
		int addrMask;
		int memPntr;
		byte *wave;
		byte *ramBank;
		byte *romBank;
		
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

	
		// Relative volume between ADPCM part and FM part, 
		// value experimentally found by Manuel Bilderbeek
		static const int ADPCM_VOLUME = 356;

		// Bitmask for register 0x07
		static const int R07_RESET        = 0x01;
		//static const int R07            = 0x02;.      // not used
		//static const int R07            = 0x04;.      // not used
		static const int R07_SP_OFF       = 0x08;
		static const int R07_REPEAT       = 0x10;
		static const int R07_MEMORY_DATA  = 0x20;
		static const int R07_REC          = 0x40;
		static const int R07_START        = 0x80;
		
		//Bitmask for register 0x08
		static const int R08_ROM          = 0x01;
		static const int R08_64K          = 0x02;
		static const int R08_DA_AD        = 0x04;
		static const int R08_SAMPL        = 0x08;
		//static const int R08            = 0x10;.      // not used
		//static const int R08            = 0x20;.      // not used
		static const int R08_NOTE_SET     = 0x40;
		static const int R08_CSM          = 0x80;

		static const int DMAX = 0x6000;
		static const int DMIN = 0x7F;
		static const int DDEF = 0x7F;
		
		static const int DECODE_MAX = 32767;
		static const int DECODE_MIN = -32768;

		static const int GETA_BITS  = 14;
		static const unsigned int MAX_STEP   = 1<<(16+GETA_BITS);
};


#endif 
