// $Id$

//-----------------------------------------------------------------------------
// 
// On Mon, 24 Feb 2003, Jon De Schrijder wrote:
// 
// I've done some measurements with the scope on the output of the SCC.
// I didn't do timing tests, only amplitude checks:
// 
// I know now for sure, the amplitude calculation works as follows:
// 
// AmpOut=640+AmpA+AmpB+AmpC+AmpD+AmpE
// 
// range AmpOut (11 bits positive number=SCC digital output): [+40...+1235]
// 
// AmpA="((SampleValue*VolA) AND #7FF0) div 16"
// AmpB="((SampleValue*VolB) AND #7FF0) div 16"
// AmpC="((SampleValue*VolC) AND #7FF0) div 16"
// AmpD="((SampleValue*VolD) AND #7FF0) div 16"
// AmpE="((SampleValue*VolE) AND #7FF0) div 16"
// 
// Setting the enablebit to zero, corresponds with VolX=0.
// 
// SampleValue range [-128...+127]
// VolX range [0..15]
// 
// Notes: 
// * SampleValue*VolX is calculated (signed multiplication) and the lower 4
//   bits are dropped (both in case the value is positive or negative), before
//   the addition of the 5 values is done. This was tested by setting
//   SampleValue=+1 and VolX=15 of different channels. The resulting AmpOut=640,
//   indicating that the 4 lower bits were dropped *before* the addition.
// 
//-----------------------------------------------------------------------------
//
// On Mon, 14 Apr 2003, Manuel Pazos wrote
// 
// I have some info about SCC/SCC+ that I hope you find useful. It is about
// "Mode Setting Register", also called "Deformation Register" Here it goes:
//
//    bit0: 4 bits frequency (%XXXX00000000). Equivalent to
//          (normal frequency >> 8) bits0-7 are ignored
//    bit1: 8 bits frequency (%0000XXXXXXXX) bits8-11 are ignored
//    bit2:
//    bit3:
//    bit4:
//    bit5: wave data is played from begining when frequency is changed
//    bit6: rotate all waves data. You can't write to them. Rotation speed
//          =3.58Mhz / (channel i frequency + 1)
//    bit7: rotate channel 4 wave data. You can't write to that channel
//          data.ONLY works in MegaROM SCC (not in SCC+)
//
// If bit7 and bit6 are set, only channel 1-3 wave data rotates . You can't
// write to ANY wave data. And there is a weird behaviour in this setting. It
// seems SCC sound is corrupted in anyway with MSX databus or so. Try to
// activate them (with proper waves, freqs, and vol.) and execute DIR command
// on DOS. You will hear "noise" This seems to be fixed in SCC+
//
// Reading Mode Setting Register, is equivalent to write #FF to it.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// 
// Additions:
//   - Setting both bit0 and bit1 is equivalent to setting only bit1
//   - A rotation goes like this:
//       wavedata[0:31] = wavedata[1:31].wavedata[0]
//   - Channel 4-5 rotation speed is set by channel 5 freq (channel 4 freq
//     is ignored for rotation)
//
//-----------------------------------------------------------------------------

#include "SCC.hh"
#include "Mixer.hh"
#include "Scheduler.hh"
#include "Debugger.hh"

namespace openmsx {

SCC::SCC(const string& name_, short volume, const EmuTime& time,
         ChipMode mode)
	: sccDebuggable(*this), currentChipMode(mode), name(name_)
{
	// Register as a soundevice
	int bufSize = Mixer::instance().registerSound(this, volume,
	                                               Mixer::MONO);
	buffer = new int[bufSize];

	// clear wave forms
	for (unsigned i = 0; i < 5; ++i) {
		for (unsigned j = 0; j < 32; ++j) {
			wave[i][j] = 0;
		}
	}
	
	reset(time);
	Debugger::instance().registerDebuggable(name, sccDebuggable);
}

SCC::~SCC()
{
	Debugger::instance().unregisterDebuggable(name, sccDebuggable);
	Mixer::instance().unregisterSound(this);
	delete[] buffer;
}

const string& SCC::getName() const
{
	return name;
}

const string& SCC::getDescription() const
{
	static const string desc_scc ("Konami SCC");
	static const string desc_sccp("Konami SCC+");
	return (currentChipMode == SCC_Real) ? desc_scc
	                                     : desc_sccp;
}

void SCC::reset(const EmuTime& time)
{
	if (currentChipMode != SCC_Real) {
		setChipMode(SCC_Compatible);
	}

	for (unsigned i = 0; i < 5; ++i) {
		for (unsigned j = 0; j < 32; ++j) {
			// don't clear wave forms
			volAdjustedWave[i][j] = 0;
		}
		count[i] = 0;
		freq[i] = 0;
		volume[i] = 0;
		rotate[i] = false;
		readOnly[i] = false;
		offset[i] = 0;
	}
	deformValue = 0;
	ch_enable = 0xFF;
	scctime = 0;

	checkMute();
}

void SCC::setSampleRate(int sampleRate)
{
	realstep = (unsigned)(((unsigned)(1 << 31)) / sampleRate);
}

void SCC::setVolume(int maxVolume)
{
	masterVolume = maxVolume;
}

void SCC::setChipMode(ChipMode newMode)
{
	if (currentChipMode == SCC_Real) {
		assert(newMode == SCC_Real);
	} else {
		assert(newMode != SCC_Real);
	}
	currentChipMode = newMode;
}

byte SCC::readMemInterface(byte address, const EmuTime& time)
{
	byte result;
	switch (currentChipMode) {
	case SCC_Real:
		if (address < 0x80) {
			// read wave form 1..4
			result = readWave(address >> 5, address, time);
		} else if (address < 0xA0) {
			// freq volume block
			result = getFreqVol(address);
		} else if (address < 0xE0) {
			// no function
			result = 0xFF;
		} else {
			// deformation register
			setDeformReg(0xFF, time);
			result = 0xFF;
		}
		break;
	case SCC_Compatible:
		if (address < 0x80) {
			// read wave form 1..4
			result = readWave(address >> 5, address, time);
		} else if (address < 0xA0) {
			// freq volume block
			result = getFreqVol(address);
		} else if (address < 0xC0) {
			// read wave form 5
			result = readWave(4, address, time);
		} else if (address < 0xE0) {
			// deformation register
			setDeformReg(0xFF, time);
			result = 0xFF;
		} else {
			// no function
			result = 0xFF;
		}
		break;
	case SCC_plusmode:
		if (address < 0xA0) {
			// read wave form 1..5
			result = readWave(address >> 5, address, time);
		} else if (address < 0xC0) {
			// freq volume block
			result = getFreqVol(address);
		} else if (address < 0xE0) {
			// deformation register
			setDeformReg(0xFF, time);
			result = 0xFF;
		} else {
			// no function
			result = 0xFF;
		}
		break;
	default:
		assert(false);
		result = 0xFF;
	}
	//PRT_DEBUG("SCC: read " << (int)address << " " << (int)result);
	return result;
}

byte SCC::readWave(byte channel, byte address, const EmuTime& time)
{
	if (!rotate[channel]) {
		return wave[channel][address & 0x1F];
	} else {
		unsigned ticks = deformTime.getTicksTill(time);
		unsigned f = ((channel == 3) && (currentChipMode != SCC_plusmode)) ?
			freq[4] : freq[channel];
		unsigned shift = ticks / (f + 1);
		return wave[channel][(address + shift) & 0x1F];
	}
}


byte SCC::getFreqVol(byte address)
{
	address &= 0x0F;
	if (address < 0x0A) {
		// get frequency
		byte channel = address / 2;
		if (address & 1) {
			return freq[channel] >> 8;
		} else {
			return freq[channel] & 0xFF;
		}
	} else if (address < 0x0F) {
		// get volume
		return volume[address - 0xA];
	} else {
		// get enable-bits
		return ch_enable;
	}
}

void SCC::writeMemInterface(byte address, byte value, const EmuTime& time)
{
	Mixer::instance().updateStream(time);

	switch (currentChipMode) {
	case SCC_Real:
		if (address < 0x80) {
			// write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// freq volume block
			setFreqVol(address, value);
		} else if (address < 0xE0) {
			// no function
		} else {
			// deformation register
			setDeformReg(value, time);
		}
		break;
	case SCC_Compatible:
		if (address < 0x80) {
			// write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// freq volume block
			setFreqVol(address, value);
		} else if (address < 0xC0) {
			// ignore write wave form 5
		} else if (address < 0xE0) {
			// deformation register
			setDeformReg(value, time);
		} else {
			// no function
		}
		break;
	case SCC_plusmode:
		if (address < 0xA0) {
			// write wave form 1..5
			writeWave(address >> 5, address, value);
		} else if (address < 0xC0) {
			// freq volume block
			setFreqVol(address, value);
		} else if (address < 0xE0) {
			// deformation register
			setDeformReg(value, time);
		} else {
			// no function
		}
		break;
	default:
		assert(false);
	}
}

void SCC::writeWave(byte channel, byte address, byte value)
{
	if (!readOnly[channel]) {
		byte pos = address & 0x1F;
		wave[channel][pos] = value;
		int tmp = ((signed_byte)value * volume[channel]) / 16;
		volAdjustedWave[channel][pos] = (tmp * masterVolume) / 256;
		if ((currentChipMode != SCC_plusmode) && (channel == 3)) {
			// copy waveform 4 -> waveform 5
			wave[4][pos] = wave[3][pos];
			volAdjustedWave[4][pos] = volAdjustedWave[3][pos];
		}
	}
}

void SCC::setFreqVol(byte address, byte value)
{
	address &= 0x0F; // regio is twice visible
	if (address < 0x0A) {
		// change frequency
		byte channel = address / 2;
		if (address & 1) {
			freq[channel] = ((value & 0xF) << 8) |
			                (freq[channel] & 0xFF);
		} else {
			freq[channel] = (freq[channel] & 0xF00) |
			                (value & 0xFF);
		}
		if (deformValue & 0x20) {
			count[channel] = 0;
		}
		unsigned frq = freq[channel];
		if (deformValue & 2) {
			// 8 bit frequency
			frq &= 0xFF;
		} else if (deformValue & 1) {
			// 4 bit frequency
			frq >>= 8;
		}
		incr[channel] = (frq <= 8) ? 0 : (2 << GETA_BITS) / (frq + 1);
	} else if (address < 0x0F) {
		// change volume
		byte channel = address - 0x0A;
		volume[channel] = value & 0xF;
		for (int i = 0; i < 32; ++i) {
			int tmp = ((signed_byte)wave[channel][i] * volume[channel]) / 16;
			volAdjustedWave[channel][i] = (tmp * masterVolume) / 256;
		}
		checkMute();
	} else {
		// change enable-bits
		ch_enable = value;
		checkMute();
	}
}

void SCC::setDeformReg(byte value, const EmuTime& time)
{
	if (value == deformValue) {
		return;
	}
	deformValue = value;
	deformTime = time;
	
	if (currentChipMode != SCC_Real) {
		value &= ~0x80;
	}
	switch (value & 0xC0) {
		case 0x00:
			for (unsigned i = 0; i < 5; ++i) {
				rotate[i] = false;
				readOnly[i] = false;
				offset[i] = 0;
			}
			break;
		case 0x40:
			for (unsigned i = 0; i < 5; ++i) {
				rotate[i] = true;
				readOnly[i] = true;
			}
			break;
		case 0x80:
			for (unsigned i = 0; i < 3; ++i) {
				rotate[i] = false;
				readOnly[i] = false;
			}
			for (unsigned i = 3; i < 5; ++i) {
				rotate[i] = true;
				readOnly[i] = true;
			}
			break;
		case 0xC0:
			for (unsigned i = 0; i < 3; ++i) {
				rotate[i] = true;
				readOnly[i] = true;
			}
			for (unsigned i = 3; i < 5; ++i) {
				rotate[i] = false;
				readOnly[i] = true;
			}
			break;
		default:
			assert(false);
	}
}



int *SCC::updateBuffer(int length)
{
	if (isInternalMuted()) {
		return NULL;
	}

	int* buf = buffer;
	if ((deformValue & 0xC0) == 0x00) {
		// No rotation stuff, this is almost always true. So it makes
		// sense to have a special optimized routine for this case
		while (length--) {
			scctime += realstep;
			unsigned advance = scctime / SCC_STEP;
			scctime %= SCC_STEP;
			int mixed = 0;
			byte enable = ch_enable;
			for (int i = 0; i < 5; ++i, enable >>= 1) {
				count[i] += incr[i] * advance;
				if (enable & 1) {
					mixed += volAdjustedWave[i]
					      [(count[i] >> GETA_BITS) & 0x1F];
				}
			}
			*buf++ = mixed;
		}
	} else {
		// Rotation mode
		//  TODO not completely correct
		while (length--) {
			scctime += realstep;
			unsigned advance = scctime / SCC_STEP;
			scctime %= SCC_STEP;
			int mixed = 0;
			byte enable = ch_enable;
			for (int i = 0; i < 5; ++i, enable >>= 1) {
				count[i] += incr[i] * advance;
				int overflows = count[i] >> (GETA_BITS + 5);
				count[i] &= ((1 << (GETA_BITS + 5)) -1);
				if (rotate[i]) {
					offset[i] += overflows;
				}
				if (enable & 1) {
					byte pos = (count[i] >> GETA_BITS) + offset[i];
					mixed += volAdjustedWave[i][pos & 0x1F];
				}
			}
			*buf++ = mixed;
		}
	}
	return buffer;
}

void SCC::checkMute()
{
	// SCC is muted unless an enabled channel with non-zero volume exists.
	bool mute = true;
	byte enable = ch_enable & 0x1F;
	byte* volumePtr = volume;
	while (enable) {
		if ((enable & 1) && *volumePtr) {
			mute = false;
			break;
		}
		enable >>= 1;
		++volumePtr;
	}
	setInternalMute(mute);
}


// class SCCDebuggable

SCC::SCCDebuggable::SCCDebuggable(SCC& parent_)
	: parent(parent_)
{
}

unsigned SCC::SCCDebuggable::getSize() const
{
	return 0x100;
}

const string& SCC::SCCDebuggable::getDescription() const
{
	static const string desc = "SCC registers in SCC+ format";
	return desc;
}

byte SCC::SCCDebuggable::read(unsigned address)
{
	if (address < 0xA0) {
		// read wave form 1..5
		const EmuTime& time = Scheduler::instance().getCurrentTime();
		return parent.readWave(address >> 5, address, time);
	} else if (address < 0xC0) {
		// freq volume block
		return parent.getFreqVol(address);
	} else if (address < 0xE0) {
		// peek deformation register
		return parent.deformValue;
	} else {
		return 0xFF;
	}
}

void SCC::SCCDebuggable::write(unsigned address, byte value)
{
	if (address < 0xA0) {
		// read wave form 1..5
		parent.writeWave(address >> 5, address, value);
	} else if (address < 0xC0) {
		// freq volume block
		 parent.setFreqVol(address, value);
	} else if (address < 0xE0) {
		// deformation register
		const EmuTime& time = Scheduler::instance().getCurrentTime();
		parent.setDeformReg(value, time);
	} else {
		// ignore
	}
}

} // namespace openmsx
