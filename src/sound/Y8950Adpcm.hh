// $Id$

#ifndef __Y8950ADPCM_HH__
#define __Y8950ADPCM_HH__

// Forward declarartions
class EmuTime;
class Y8950;

#include "../openmsx.hh"


class Y8950Adpcm
{
	public:
		Y8950Adpcm(Y8950 *y8950, int sampleRam);
		~Y8950Adpcm();
		
		void reset();
		void setSampleRate(int sr);
		bool muted();
		void writeReg(byte rg, byte data, const EmuTime &time);
		byte readReg(byte rg);
		int calcSample();

	private:
		int CLAP(int min, int x, int max);
		void restart();

		Y8950 *y8950;

		int sampleRate;
		
		int ramSize;
		byte *wave;
		byte *ramBank;
		byte *romBank;
		int startAddr;
		int stopAddr;
		int playAddr;
		int memPntr;
		int addrMask;
		
		bool playing;
		byte volume;
		word delta;
		int deltaN;
		int deltaAddr;
		int lastOut, prevOut;
		int diff;
		
		byte reg7;
		byte reg15;

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

		static const int DMAX = 0x5FFF;
		static const int DMIN = 0x7F;
		static const int DDEF = 0x7F;
		
		static const int DECODE_MAX = 32767;
		static const int DECODE_MIN = -32768;

		static const int GETA_BITS       = 14;
		static const int DELTA_ADDR_MAX  = 1<<(16+GETA_BITS);
};


#endif 
