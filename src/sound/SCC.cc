// $Id$

#include "SCC.hh"
#include "Mixer.hh"

SCC::SCC(short volume)
{
	PRT_DEBUG("instantiating an SCC object");
	// Register as a soundevice
	int bufSize = Mixer::instance()->registerSound(this);
	buffer = new int[bufSize];
	setVolume(volume);
	reset();
}

SCC::~SCC()
{
	PRT_DEBUG("Destructing an SCC object");
	Mixer::instance()->unregisterSound(this);
}

byte SCC::readMemInterface(byte address,const EmuTime &time)
{
	return memInterface[address];
}

void SCC::setChipMode(ChipMode chip)
{
	if (currentChipMode==chip)
		return;
	currentChipMode = chip;
	// lower 128 bytes are always the same
	switch (chip) {
		case SCC_Real:
			// Simply mask upper 128 bytes
			for (int i=0x80; i<=0xFF; i++)
				memInterface[i] = 0xFF;
			break;
		case SCC_Compatible:
			// 32 bytes Frequency Volume data
			getFreqVol(0x80);
			// 32 bytes waveform five
			for (int i=0; i<=0x1F; i++)
				memInterface[0xA0+i] = wave[4][i];
			// 32 bytes deform register
			getDeform(0xC0);
			// no function area
			for (int i=0xE0; i<=0xFF; i++)
				memInterface[i] = 0xFF;
			break;
		case SCC_plusmode:
			// 32 bytes waveform five
			for (int i=0; i<=0x1F; i++)
				memInterface[0x80+i] = wave[4][i];
			// 32 bytes Frequency Volume data
			getFreqVol(0xA0);
			// 32 bytes deform register
			getDeform(0xC0);
			// no function area
			for (int i=0xE0; i<=0xFF; i++)
				memInterface[i] = 0xFF;
	}
}

void SCC::getDeform(byte offset)
{
	// 32 bytes deform register
	for (int i=0; i<=0x1F; i++)
		memInterface[i + offset] = deformationRegister;
}

void SCC::getFreqVol(byte offset)
{
	// 32 bytes Frequency Volume data
	// Take care of the mirroring
	for (int i=0; i<10; i++) {
		memInterface[offset + i]      =
		memInterface[offset + i + 16] =
			i&1 ? freq[i>>1]>>8 : freq[i>>1]&0xFF;
	}
	for (int i = 0; i < 5; i++) {
		memInterface[offset + i + 10]      =
		memInterface[offset + i + 10 + 16] =
			volume[i];
	}
	memInterface[offset + 15] =
	memInterface[offset + 31] =
		ch_enable;
}


void SCC::writeMemInterface(byte address, byte value, const EmuTime &time)
{
	Mixer::instance()->updateStream(time);

	byte waveborder = (currentChipMode == SCC_plusmode) ? 0xA0 : 0x80;
	if (address < waveborder) {
		// waveform info
		// note: extra 32 bytes in SCC+ mode
		int ch = address>>5;
		//Don't know why but japanese doesn't change wave when noise is enabled ??
		//if (!rotate[ch]) {
		wave[ch][address&0x1f]            = value;
		volAdjustedWave[ch][address&0x1f] = ((signed_byte)value*volume[ch]*masterVolume)/2048;
		if ((currentChipMode != SCC_plusmode) && (ch==3)) {
			// copy waveform 4 -> waveform 5
			wave[4][address&0x1f]            = wave[ch][address&0x1f];
			volAdjustedWave[4][address&0x1f] = volAdjustedWave[ch][address&0x1f];
			if (currentChipMode == SCC_Compatible)
				memInterface[address+64] = value;
		}
		//} Doing this would conflict with wave5=4 meminterface from 2 lines above
		// TODO: Need to figure this noise thing out
		memInterface[address] = value;
		return;
	}
	switch (currentChipMode) {
		case SCC_Real:
			if (address < 0xA0) {
				setFreqVol(value, address-0x80);
			} else if (address >= 0xE0) {
				setDeformReg(value);
			}
			break;
		case SCC_Compatible:
			if (address < 0xA0) {
				setFreqVol(value, address-0x80);
				memInterface[address | 0x10] = value;
				memInterface[address & 0xEF] = value;
			} else if ((address >= 0xC0) && (address < 0xE0)) {
				setDeformReg(value);
				for (int i=0xC0; i<=0xE0; i++)
					memInterface[i] = value;
			}
			break;
		case SCC_plusmode:
			if (address < 0xC0) {
				setFreqVol(value, address-0xA0);
				memInterface[address | 0x10] = value;
				memInterface[address & 0xEF] = value;
			} else if ((address >= 0xC0) && (address < 0xE0)) {
				setDeformReg(value);
				for (int i=0xC0; i<=0xE0; i++)
					memInterface[i] = value;
			}
	}
}

void SCC::setDeformReg(byte value)
{
	deformationRegister = value;
	cycle_4bit = value&1;
	cycle_8bit = value&2;
	refresh = value&32;
	/* Code taken from the japanese guy
	   Didn't take time to integrate in my method so far
	   According to sean these bits should produce noise on the channels

	   if (value&64)
	   	for(int ch=0;ch<5;ch++)
	   		rotate[ch] = 0x1F;
	   else
	   	for(int ch=0;ch<5;ch++)
	   		rotate[ch] = 0;
	   if (value&128)
	   	rotate[3] = rotate[4] = 0x1F;
	 */
}

void SCC::setFreqVol(byte value, byte address)
{
	address &= 0x0f; //regio is twice visible
	if (address < 0x0a) {
		// change frequency
		byte channel = address/2;
		if (address&1)
			freq[channel] = ((value&0xf)<<8) | (freq[channel]&0xff);
		else
			freq[channel] = (freq[channel]&0xf00) | (value&0xff);
		if (refresh)
			count[channel] = 0;
		unsigned frq = freq[channel];
		if (cycle_8bit)
			frq &= 0xff;
		if (cycle_4bit)
			frq >>= 8;
		incr[channel] = (frq <= 8) ? 0 : (2<<GETA_BITS)/(frq+1);
	}
	else if (address < 0x0f) {
		// change volume
		byte channel = address-0x0a;
		volume[channel] = value&0xf;
		for (int i=0; i<32; i++)
			volAdjustedWave[channel][i] = ((signed_byte)(wave[channel][i])*volume[channel]*masterVolume)/2048;
		checkMute();
	} else {
		// change enable-bits
		ch_enable = value;
		checkMute();
	}
}


void SCC::reset()
{
	//TODO update memInterface[]
	
	currentChipMode = SCC_Real;
	deformationRegister = 0;

	for(int i=0; i<5; i++) {
		for(int j=0; j<32 ; j++) {
			wave[i][j] = 0;
			volAdjustedWave[i][j] = 0;
		}
		count[i] = 0;
		freq[i] = 0;
		volume[i] = 0;
		//rotate[i] = 0;
	}
	ch_enable = 0xff;
	cycle_4bit = false;
	cycle_8bit = false;
	refresh = false;
	scctime = 0;

	checkMute();
}

void SCC::setSampleRate(int sampleRate)
{
	realstep = (unsigned)(((unsigned)(1<<31))/sampleRate);
}

void SCC::setInternalVolume(short maxVolume)
{
	masterVolume = maxVolume;
}

int *SCC::updateBuffer(int length)
{
	PRT_DEBUG("SCC: updateBuffer called ");
	if (isInternalMuted()) {
		PRT_DEBUG("SCC: muted");
		return NULL;
	}

	int *buf = buffer;
	while (length--) {
		scctime += realstep;
		unsigned advance = scctime / SCC_STEP;
		scctime %= SCC_STEP;
		int mixed = 0;
		unsigned enable = ch_enable;
		for (int channel = 0; channel < 5; channel++, enable >>= 1) {
			if (enable & 1) {
				mixed += volAdjustedWave[channel]
					[(count[channel] >> (GETA_BITS)) & 0x1F];
			}
			count[channel] += incr[channel] * advance;
		}
		*buf++ = mixed;
	}
	return buffer;
}

void SCC::checkMute()
{
	// SCC is mute unless an enabled channel with non-zero volume exists.
	bool mute = true;
	unsigned enable = ch_enable & 0x1F;
	byte *volumePtr = volume;
	while (enable) {
		if ((enable & 1) && *volumePtr) {
			mute = false;
			break;
		}
		enable >>= 1;
		volumePtr++;
	}
	PRT_DEBUG("SCC+: setInternalMute(" << mute << "); ");
	setInternalMute(mute);
}
