// $Id$

#ifndef __SCC_HH__
#define __SCC_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"

class EmuTime;


class SCC : public SoundDevice
{
	public:
		enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};
		
		SCC(short volume, const EmuTime &time);
		virtual ~SCC();

		// interaction with realCartridge
		void reset(const EmuTime &time);
		byte readMemInterface(byte address,const EmuTime &time);
		void writeMemInterface(byte address, byte value,
		                       const EmuTime &time);
		void setChipMode(ChipMode chip);

		// SoundDevice
		virtual void setSampleRate(int sampleRate);
		virtual void setInternalVolume(short maxVolume);
		virtual int* updateBuffer(int length);

	private:
		inline void checkMute();
		void setDeformReg(byte value);
		void setFreqVol(byte value, byte address);
		void getDeform(byte offset);
		void getFreqVol(byte offset);

		static const int GETA_BITS = 22;
		static const int CLOCK_FREQ = 3579545;
		static const unsigned SCC_STEP =
			(unsigned)(((unsigned)(1 << 31)) / (CLOCK_FREQ / 2));

		ChipMode currentChipMode;
		int* buffer;
		int masterVolume;
		unsigned realstep;
		unsigned scctime;

		byte scc_banked;
		byte deformationRegister;
		char wave[5][32];
		int volAdjustedWave[5][32];
		unsigned incr[5];
		unsigned count[5];
		unsigned freq[5];
		byte volume[5];
		
		//int rotate[5];	// TODO what exactly does this do?
		bool cycle_4bit;
		bool cycle_8bit;
		bool refresh;
		
		byte ch_enable;
		byte memInterface[256];
};

#endif //__SCC_HH__
