// $Id$

#ifndef __SCC_HH__
#define __SCC_HH__

#include <string>
#include "openmsx.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"

using std::string;

class SCC : public SoundDevice
{
	public:
		enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};
		
		SCC(const string &name, short volume, const EmuTime &time,
		    ChipMode mode = SCC_Real);
		virtual ~SCC();

		// interaction with realCartridge
		void reset(const EmuTime &time);
		byte readMemInterface(byte address,const EmuTime &time);
		void writeMemInterface(byte address, byte value,
		                       const EmuTime &time);
		void setChipMode(ChipMode newMode);

		// SoundDevice
		virtual void setSampleRate(int sampleRate);
		virtual void setInternalVolume(short maxVolume);
		virtual int* updateBuffer(int length);

	private:
		inline void checkMute();
		byte readWave(byte channel, byte address, const EmuTime &time);
		void writeWave(byte channel, byte offset, byte value);
		void setDeformReg(byte value, const EmuTime &time);
		void setFreqVol(byte address, byte value);
		byte getFreqVol(byte address);

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
		byte ch_enable;
		
		byte deformValue;
		EmuTimeFreq<CLOCK_FREQ> deformTime;
		bool rotate[5];
		bool readOnly[5];
		byte offset[5];
};

#endif //__SCC_HH__
