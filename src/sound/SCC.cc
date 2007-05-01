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
//
// On Sat, 09 Sep 2005, NYYRIKKI wrote (MRC post)
//
// ...
//
// One important thing to know is that change of volume is not implemented
// immediately in SCC. Normally it is changed when next byte from sample memory
// is played, but writing value to frequency causes current byte to be started
// again. As in this example we write values very quickly to frequency registers
// the internal sample counter does not actually move at all.
//
// Third method is a variation of first method. As we don't know where SCC is
// playing, let's update the whole sample memory with one and same new value.
// To make sample rate not variable in low sample rates we first stop SCC from
// reading sample memory. This can be done by writing value less than 9 to
// frequency. Now we can update sample RAM so, that output does not change.
// After sample RAM has been updated, we start SCC internal counter so that
// value (where ever the counter was) is sent to output. This routine can be
// found below as example 3.
//
// ...
//
//
//
// Something completely different: the SCC+ is actually called SCC-I.
//-----------------------------------------------------------------------------

#include "SCC.hh"
#include "SimpleDebuggable.hh"
#include "MSXMotherBoard.hh"
#include <cmath>

#include <iostream>

using std::string;

namespace openmsx {

class SCCDebuggable : public SimpleDebuggable
{
public:
	SCCDebuggable(MSXMotherBoard& motherBoard, SCC& scc);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	SCC& scc;
};


static string calcDescription(SCC::ChipMode mode)
{
	return (mode == SCC::SCC_Real) ? "Konami SCC" : "Konami SCC+";
}

SCC::SCC(MSXMotherBoard& motherBoard, const string& name,
         const XMLElement& config, const EmuTime& time, ChipMode mode)
	: SoundDevice(motherBoard.getMSXMixer(), name, calcDescription(mode), 5)
	, debuggable(new SCCDebuggable(motherBoard, *this))
	, deformTimer(time)
	, currentChipMode(mode)
{
	// Clear all state that is unaffected by reset.
	for (unsigned i = 0; i < 5; ++i) {
		for (unsigned j = 0; j < 32; ++j) {
			wave[i][j] = 0;
			volAdjustedWave[i][j] = 0;
		}
		out[i] = 0;
		count[i] = 0;
		pos[i] = (unsigned)-1;
		freq[i] = 0;
		volume[i] = 0;
	}

	reset(time);
	registerSound(config);
}

SCC::~SCC()
{
	unregisterSound();
}

void SCC::reset(const EmuTime& /*time*/)
{
	if (currentChipMode != SCC_Real) {
		setChipMode(SCC_Compatible);
	}

	for (unsigned i = 0; i < 5; ++i) {
		rotate[i] = false;
		readOnly[i] = false;
	}
	deformValue = 0;
	ch_enable = 0;
}

void SCC::setOutputRate(unsigned sampleRate)
{
	double input = 3579545.0 / 32;
	setInputRate(static_cast<int>(input + 0.5));
	setResampleRatio(input, sampleRate);
}

void SCC::setVolume(int maxVolume)
{
	masterVolume = 2 * maxVolume;
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
		} else if (address < 0xE0) {
			// 0x80..0x9F : freq volume block, write only
			// 0xA0..0xDF : no function
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
			result = 0xFF;
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
			result = 0xFF;
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
		unsigned ticks = deformTimer.getTicksTill(time);
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
	updateStream(time);

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

inline int SCC::adjust(signed char wav, byte vol)
{
	int tmp = (static_cast<int>(wav) * vol) / 16;
	return (tmp * masterVolume) / 256;
}

void SCC::writeWave(byte channel, byte address, byte value)
{
	// write to channel 5 only possible in SCC+ mode
	assert(channel < 5);
	assert((channel != 4) || (currentChipMode == SCC_plusmode));

	if (!readOnly[channel]) {
		byte pos = address & 0x1F;
		wave[channel][pos] = value;
		volAdjustedWave[channel][pos] = adjust(value, volume[channel]);
		if ((currentChipMode != SCC_plusmode) && (channel == 3)) {
			// copy waveform 4 -> waveform 5
			wave[4][pos] = wave[3][pos];
			volAdjustedWave[4][pos] = adjust(value, volume[4]);
		}
	}
}

void SCC::setFreqVol(byte address, byte value)
{
	address &= 0x0F; // region is visible twice
	if (address < 0x0A) {
		// change frequency
		byte channel = address / 2;
		unsigned frq =
			  (address & 1)
			? ((value & 0xF) << 8) | (freq[channel] & 0xFF)
			: (freq[channel] & 0xF00) | (value & 0xFF);
		freq[channel] = frq;
		if (deformValue & 2) {
			// 8 bit frequency
			frq &= 0xFF;
		} else if (deformValue & 1) {
			// 4 bit frequency
			frq >>= 8;
		}
		incr[channel] = (frq <= 8) ? 0 : (1 << 28) / (frq + 1);
		if (deformValue & 0x20) {
			count[channel] = 0; // reset to begin of sample
		} else {
			count[channel] &= 0x0F800000; // reset to begin of byte
		}
		pos[channel] = (unsigned)-1;
	} else if (address < 0x0F) {
		// change volume
		byte channel = address - 0x0A;
		volume[channel] = value & 0xF;
		for (int i = 0; i < 32; ++i) {
			volAdjustedWave[channel][i] =
				adjust(wave[channel][i], volume[channel]);
		}
	} else {
		// change enable-bits
		ch_enable = value;
	}
}

void SCC::setDeformReg(byte value, const EmuTime& time)
{
	if (value == deformValue) {
		return;
	}
	deformValue = value;
	deformTimer.advance(time);

	if (currentChipMode != SCC_Real) {
		value &= ~0x80;
	}
	switch (value & 0xC0) {
		case 0x00:
			for (unsigned i = 0; i < 5; ++i) {
				rotate[i] = false;
				readOnly[i] = false;
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

void SCC::generateChannels(int** bufs, unsigned num)
{
	if ((deformValue & 0xC0) == 0x00) {
		// No rotation stuff, this is almost always true. So it makes
		// sense to have a special optimized routine for this case
		byte enable = ch_enable;
		for (int i = 0; i < 5; ++i, enable >>= 1) {
			if ((enable & 1) && volume[i]) {
				for (unsigned j = 0; j < num; ++j) {
					count[i] = (count[i] + incr[i]) & 0x0FFFFFFF;
					unsigned newPos = count[i] >> 23;
					if (newPos != pos[i]) {
						pos[i] = newPos;
						out[i] = volAdjustedWave[i][newPos];
					}
					bufs[i][j] = out[i];
				}
			} else {
				count[i] = (count[i] + num * incr[i]) & 0x0FFFFFFF;
				pos[i] = (unsigned)-1; // invalidate out[i]
				bufs[i] = 0; // channel muted
			}
		}
	} else {
		// Rotation mode
		//  TODO not completely correct
		byte enable = ch_enable;
		for (int i = 0; i < 5; ++i, enable >>= 1) {
			if (enable & 1) {
				for (unsigned j = 0; j < num; ++j) {
					unsigned cnt = count[i] + incr[i];
					if (rotate[i]) {
						cnt += (cnt >> 28) << 23;
					}
					count[i] = cnt & 0x0FFFFFFF;
					unsigned newPos = count[i] >> 23;
					if (newPos != pos[i]) {
						pos[i] = newPos;
						out[i] = volAdjustedWave[i][newPos];
					}
					bufs[i][j] = out[i];
				}
			} else {
				count[i] = (count[i] + num * incr[i]) & 0x0FFFFFFF;
				pos[i] = (unsigned)-1; // invalidate out[i]
				bufs[i] = 0; // channel muted
			}
		}
	}
}

bool SCC::generateInput(float* buffer, unsigned num)
{
	int tmpBuf[num];
	if (mixChannels(tmpBuf, num)) {
		for (unsigned i = 0; i < num; ++i) {
			buffer[i] = tmpBuf[i];
		}
		return true;
	} else {
		return false;
	}
}

bool SCC::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	float tmpBuf[length];
	if (generateOutput(tmpBuf, length)) {
		for (unsigned i = 0; i < length; ++i) {
			buffer[i] = lrintf(tmpBuf[i]);
		}
		return true;
	} else {
		return false;
	}
}


// SimpleDebuggable

SCCDebuggable::SCCDebuggable(MSXMotherBoard& motherBoard, SCC& scc_)
	: SimpleDebuggable(motherBoard, scc_.getName() + " SCC",
	                   "SCC registers in SCC+ format", 0x100)
	, scc(scc_)
{
}

byte SCCDebuggable::read(unsigned address, const EmuTime& time)
{
	if (address < 0xA0) {
		// read wave form 1..5
		return scc.readWave(address >> 5, address, time);
	} else if (address < 0xC0) {
		// freq volume block
		return scc.getFreqVol(address);
	} else if (address < 0xE0) {
		// peek deformation register
		return scc.deformValue;
	} else {
		return 0xFF;
	}
}

void SCCDebuggable::write(unsigned address, byte value, const EmuTime& time)
{
	if (address < 0xA0) {
		// read wave form 1..5
		scc.writeWave(address >> 5, address, value);
	} else if (address < 0xC0) {
		// freq volume block
		 scc.setFreqVol(address, value);
	} else if (address < 0xE0) {
		// deformation register
		scc.setDeformReg(value, time);
	} else {
		// ignore
	}
}

} // namespace openmsx
