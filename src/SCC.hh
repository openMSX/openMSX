// $Id$

#ifndef __SCC_HH__
#define __SCC_HH__

#include "openmsx.hh" //for the byte definition
#include "EmuTime.hh"
#include "SoundDevice.hh"
#include "Mixer.hh"

class SCC : public SoundDevice
{
	public:
		SCC();
		//SCC(MSXConfig::Device *config);
		virtual ~SCC();

		enum ChipMode {SCC_Real,SCC_Compatible,SCC_plusmode };

		//void start();
		//void stop();
		//void init();
		void reset();
//		void reset(const EmuTime &time);
		// interaction with realCartridge
		byte readMemInterface(byte address,const EmuTime &time);
		void writeMemInterface(byte address, byte value,const EmuTime &time);
		void setChipMode(ChipMode chip);

		//SoundDevice
		void setSampleRate(int sampleRate);
		void setInternalVolume(short maxVolume);
		int* updateBuffer(int length);

	private:
		ChipMode currentChipMode;
		inline void checkEnable();
		inline void checkMute();
		void setModeRegister(byte value);
		void setDeformReg(byte value);
		void setFreqVol(byte value, byte address);
		void getDeform(byte offset);

		void getFreqVol(byte offset);

		static const int GETA_BITS = 22;
		static const int CLOCK_FREQ = 3579545;
		static const unsigned SCC_STEP =
			(unsigned)(((unsigned)(1<<31))/(CLOCK_FREQ/2));

		int* buffer;

		int out;

		int masterVolume;

		unsigned realstep;
		unsigned scctime;

		unsigned incr[5];

		byte scc_banked;
		byte ModeRegister;
		byte DeformationRegister;

		char  wave[5][32];
		int volAdjustedWave[5][32];

		unsigned enable;

		unsigned count[5];
		unsigned freq[5];
		unsigned phase[5];
		unsigned volume[5];
		unsigned offset[5];

		int ch_enable;

		bool cycle_4bit;
		bool cycle_8bit;
		int refresh;
  /* Code taken from the japanese guy
		int rotate[5];
  */
		byte memInterface[256];
};
#endif //__SCC_HH__
