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
// Also see this MRC thread:
//  http://www.msx.org/forumtopicl7875.html
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
#include "DeviceConfig.hh"
#include "serialize.hh"
#include "likely.hh"
#include "outer.hh"
#include "unreachable.hh"

using std::string;

namespace openmsx {

static string calcDescription(SCC::ChipMode mode)
{
	return (mode == SCC::SCC_Real) ? "Konami SCC" : "Konami SCC+";
}

SCC::SCC(const string& name_, const DeviceConfig& config,
         EmuTime::param time, ChipMode mode)
	: ResampledSoundDevice(
		config.getMotherBoard(), name_, calcDescription(mode), 5)
	, debuggable(config.getMotherBoard(), getName())
	, deformTimer(time)
	, currentChipMode(mode)
{
	// Make valgrind happy
	for (auto& op : orgPeriod) op = 0;

	float input = 3579545.0f / 32;
	setInputRate(int(input + 0.5f));

	powerUp(time);
	registerSound(config);
}

SCC::~SCC()
{
	unregisterSound();
}

void SCC::powerUp(EmuTime::param time)
{
	// Power on values, tested by enen (log from IRC #openmsx):
	//
	//  <enen>    wouter_: i did an scc poweron values test, deform=0,
	//            amplitude=full, channelenable=0, period=under 8
	//    ...
	//  <wouter_> did you test the value of the waveforms as well?
	//    ...
	//  <enen>    filled with $FF, some bits cleared but that seems random

	// Initialize ch_enable, deform (initialize this before period)
	reset(time);

	// Initialize waveform (initialize before volumes)
	for (unsigned i = 0; i < 5; ++i) {
		for (unsigned j = 0; j < 32; ++j) {
			wave[i][j] = ~0;
		}
	}
	// Initialize volume (initialize this before period)
	for (int i = 0; i < 5; ++i) {
		setFreqVol(i + 10, 15, time);
	}
	// Actual initial value is difficult to measure, assume zero
	// (initialize before period)
	for (auto& p : pos) p = 0;

	// Initialize period (sets members orgPeriod, period, incr, count, out)
	for (int i = 0; i < 2 * 5; ++i) {
		setFreqVol(i, 0, time);
	}
}

void SCC::reset(EmuTime::param /*time*/)
{
	if (currentChipMode != SCC_Real) {
		setChipMode(SCC_Compatible);
	}

	setDeformRegHelper(0);
	ch_enable = 0;
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

byte SCC::readMem(byte addr, EmuTime::param time)
{
	// Deform-register locations:
	//   SCC_Real:       0xE0..0xFF
	//   SCC_Compatible: 0xC0..0xDF
	//   SCC_plusmode:   0xC0..0xDF
	if (((currentChipMode == SCC_Real) && (addr >= 0xE0)) ||
	    ((currentChipMode != SCC_Real) && (0xC0 <= addr) && (addr < 0xE0))) {
		setDeformReg(0xFF, time);
	}
	return peekMem(addr, time);
}

byte SCC::peekMem(byte address, EmuTime::param time) const
{
	byte result;
	switch (currentChipMode) {
	case SCC_Real:
		if (address < 0x80) {
			// 0x00..0x7F : read wave form 1..4
			result = readWave(address >> 5, address, time);
		} else {
			// 0x80..0x9F : freq volume block, write only
			// 0xA0..0xDF : no function
			// 0xE0..0xFF : deformation register
			result = 0xFF;
		}
		break;
	case SCC_Compatible:
		if (address < 0x80) {
			// 0x00..0x7F : read wave form 1..4
			result = readWave(address >> 5, address, time);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			result = 0xFF;
		} else if (address < 0xC0) {
			// 0xA0..0xBF : read wave form 5
			result = readWave(4, address, time);
		} else {
			// 0xC0..0xDF : deformation register
			// 0xE0..0xFF : no function
			result = 0xFF;
		}
		break;
	case SCC_plusmode:
		if (address < 0xA0) {
			// 0x00..0x9F : read wave form 1..5
			result = readWave(address >> 5, address, time);
		} else {
			// 0xA0..0xBF : freq volume block
			// 0xC0..0xDF : deformation register
			// 0xE0..0xFF : no function
			result = 0xFF;
		}
		break;
	default:
		UNREACHABLE; return 0;
	}
	return result;
}

byte SCC::readWave(unsigned channel, unsigned address, EmuTime::param time) const
{
	if (!rotate[channel]) {
		return wave[channel][address & 0x1F];
	} else {
		unsigned ticks = deformTimer.getTicksTill(time);
		unsigned periodCh = ((channel == 3) &&
		                     (currentChipMode != SCC_plusmode) &&
		                     ((deformValue & 0xC0) == 0x40))
		                  ? 4 : channel;
		unsigned shift = ticks / (period[periodCh] + 1);
		return wave[channel][(address + shift) & 0x1F];
	}
}


byte SCC::getFreqVol(unsigned address) const
{
	address &= 0x0F;
	if (address < 0x0A) {
		// get frequency
		unsigned channel = address / 2;
		if (address & 1) {
			return orgPeriod[channel] >> 8;
		} else {
			return orgPeriod[channel] & 0xFF;
		}
	} else if (address < 0x0F) {
		// get volume
		return volume[address - 0xA];
	} else {
		// get enable-bits
		return ch_enable;
	}
}

void SCC::writeMem(byte address, byte value, EmuTime::param time)
{
	updateStream(time);

	switch (currentChipMode) {
	case SCC_Real:
		if (address < 0x80) {
			// 0x00..0x7F : write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			setFreqVol(address, value, time);
		} else if (address < 0xE0) {
			// 0xA0..0xDF : no function
		} else {
			// 0xE0..0xFF : deformation register
			setDeformReg(value, time);
		}
		break;
	case SCC_Compatible:
		if (address < 0x80) {
			// 0x00..0x7F : write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			setFreqVol(address, value, time);
		} else if (address < 0xC0) {
			// 0xA0..0xBF : ignore write wave form 5
		} else if (address < 0xE0) {
			// 0xC0..0xDF : deformation register
			setDeformReg(value, time);
		} else {
			// 0xE0..0xFF : no function
		}
		break;
	case SCC_plusmode:
		if (address < 0xA0) {
			// 0x00..0x9F : write wave form 1..5
			writeWave(address >> 5, address, value);
		} else if (address < 0xC0) {
			// 0xA0..0xBF : freq volume block
			setFreqVol(address, value, time);
		} else if (address < 0xE0) {
			// 0xC0..0xDF : deformation register
			setDeformReg(value, time);
		} else {
			// 0xE0..0xFF : no function
		}
		break;
	default:
		UNREACHABLE;
	}
}

int SCC::getAmplificationFactor() const
{
	return 256;
}

inline int SCC::adjust(signed char wav, byte vol)
{
	return (int(wav) * vol) >> 4;
}

void SCC::writeWave(unsigned channel, unsigned address, byte value)
{
	// write to channel 5 only possible in SCC+ mode
	assert(channel < 5);
	assert((channel != 4) || (currentChipMode == SCC_plusmode));

	if (!readOnly[channel]) {
		unsigned p = address & 0x1F;
		wave[channel][p] = value;
		volAdjustedWave[channel][p] = adjust(value, volume[channel]);
		if ((currentChipMode != SCC_plusmode) && (channel == 3)) {
			// copy waveform 4 -> waveform 5
			wave[4][p] = wave[3][p];
			volAdjustedWave[4][p] = adjust(value, volume[4]);
		}
	}
}

void SCC::setFreqVol(unsigned address, byte value, EmuTime::param time)
{
	address &= 0x0F; // region is visible twice
	if (address < 0x0A) {
		// change frequency
		unsigned channel = address / 2;
		unsigned per =
			  (address & 1)
			? ((value & 0xF) << 8) | (orgPeriod[channel] & 0xFF)
			: (orgPeriod[channel] & 0xF00) | (value & 0xFF);
		orgPeriod[channel] = per;
		if (deformValue & 2) {
			// 8 bit frequency
			per &= 0xFF;
		} else if (deformValue & 1) {
			// 4 bit frequency
			per >>= 8;
		}
		period[channel] = per;
		incr[channel] = (per <= 8) ? 0 : 32;
		count[channel] = 0; // reset to begin of byte
		if (deformValue & 0x20) {
			pos[channel] = 0; // reset to begin of waveform
			// also 'rotation' mode (confirmed by test based on
			// Artag's SCC sample player)
			deformTimer.advance(time);
		}
		// after a freq change, update the output
		out[channel] = volAdjustedWave[channel][pos[channel]];
	} else if (address < 0x0F) {
		// change volume
		unsigned channel = address - 0x0A;
		volume[channel] = value & 0xF;
		for (unsigned i = 0; i < 32; ++i) {
			volAdjustedWave[channel][i] =
				adjust(wave[channel][i], volume[channel]);
		}
	} else {
		// change enable-bits
		ch_enable = value;
	}
}

void SCC::setDeformReg(byte value, EmuTime::param time)
{
	if (value == deformValue) {
		return;
	}
	deformTimer.advance(time);
	setDeformRegHelper(value);
}

void SCC::setDeformRegHelper(byte value)
{
	deformValue = value;
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
		UNREACHABLE;
	}
}

void SCC::generateChannels(int** bufs, unsigned num)
{
	unsigned enable = ch_enable;
	for (unsigned i = 0; i < 5; ++i, enable >>= 1) {
		if ((enable & 1) && (volume[i] || out[i])) {
#ifdef __arm__
			unsigned dummy;
			int* buf = bufs[i];
			asm volatile (
			"0:\n\t"
				"ldr	%[T],[%[B]]\n\t"
				"add	%[T],%[T],%[O]\n\t"
				"add	%[C],%[C],%[I]\n\t"
				"str	%[T],[%[B]],#4\n\t"
				"subs	%[T],%[C],%[PE]\n\t"
				"bpl	2f\n"
			"1:\n\t"
				"cmp	%[B],%[E]\n\t"
				"bne	0b\n\t"
				"b	3f\n"
			"2:\n\t"
				"adds	%[PO],%[PO],#1\n\t"
				"subs	%[T],%[T],%[PE]\n\t"
				"bpl	2b\n\t"
				"and	%[PO],%[PO],#31\n\t"
				"add	%[C],%[T],%[PE]\n\t"
				"ldr	%[O],[%[V],%[PO],LSL #2]\n\t"
				"b	1b\n"
			"3:\n\t"

				: [T]  "=&r"  (dummy)
				, [B]  "=r"   (buf)
				, [O]  "=r"   (out[i])
				, [C]  "=r"   (count[i])
				, [PO] "=r"   (pos[i])
				:      "[B]"  (buf)
				,      "[O]"  (out[i])
				,      "[C]"  (count[i])
				, [I]  "r"    (incr[i])
				, [PE] "r"    (period[i] + 1)
				, [E]  "r"    (&buf[num])
				,      "[PO]" (pos[i])
				, [V]  "r"    (volAdjustedWave[i])
				: "memory", "cc"
			);
#else
			int out2 = out[i];
			unsigned count2 = count[i];
			unsigned pos2 = pos[i];
			unsigned incr2 = incr[i];
			unsigned period2 = period[i] + 1;
			for (unsigned j = 0; j < num; ++j) {
				bufs[i][j] += out2;
				count2 += incr2;
				// Note: only for very small periods
				//       this will take more than 1 iteration
				while (unlikely(count2 >= period2)) {
					count2 -= period2;
					pos2 = (pos2 + 1) % 32;
					out2 = volAdjustedWave[i][pos2];
				}
			}
			out[i] = out2;
			count[i] = count2;
			pos[i] = pos2;
#endif
		} else {
			bufs[i] = nullptr; // channel muted
			// Update phase counter.
			unsigned newCount = count[i] + num * incr[i];
			count[i] = newCount % (period[i] + 1);
			pos[i] = (pos[i] + newCount / (period[i] + 1)) % 32;
			// Channel stays off until next waveform index.
			out[i] = 0;
		}
	}
}


// Debuggable

SCC::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " SCC",
	                   "SCC registers in SCC+ format", 0x100)
{
}

byte SCC::Debuggable::read(unsigned address, EmuTime::param time)
{
	auto& scc = OUTER(SCC, debuggable);
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

void SCC::Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	auto& scc = OUTER(SCC, debuggable);
	if (address < 0xA0) {
		// read wave form 1..5
		scc.writeWave(address >> 5, address, value);
	} else if (address < 0xC0) {
		// freq volume block
		scc.setFreqVol(address, value, time);
	} else if (address < 0xE0) {
		// deformation register
		scc.setDeformReg(value, time);
	} else {
		// ignore
	}
}


static std::initializer_list<enum_string<SCC::ChipMode>> chipModeInfo = {
	{ "Real",       SCC::SCC_Real       },
	{ "Compatible", SCC::SCC_Compatible },
	{ "Plus",       SCC::SCC_plusmode   },
};
SERIALIZE_ENUM(SCC::ChipMode, chipModeInfo);

template<typename Archive>
void SCC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mode", currentChipMode);
	ar.serialize("period", orgPeriod);
	ar.serialize("volume", volume);
	ar.serialize("ch_enable", ch_enable);
	ar.serialize("deformTimer", deformTimer);
	ar.serialize("deform", deformValue);
	// multi-dimensional arrays are not directly support by the
	// serialization framework, maybe in the future. So for now
	// manually loop over the channels.
	char tag[6] = { 'w', 'a', 'v', 'e', 'X', 0 };
	for (int channel = 0; channel < 5; ++channel) {
		tag[4] = char('1' + channel);
		ar.serialize(tag, wave[channel]); // signed char
	}

	if (ar.isLoader()) {
		// recalculate volAdjustedWave
		for (int channel = 0; channel < 5; ++channel) {
			for (int p = 0; p < 32; ++p) {
				volAdjustedWave[channel][p] =
					adjust(wave[channel][p], volume[channel]);
			}
		}

		// recalculate rotate[5] and readOnly[5]
		setDeformRegHelper(deformValue);

		// recalculate incr[5] and period[5]
		//  this also (possibly) changes count[5], pos[5] and out[5]
		//  as an unwanted side-effect, so (de)serialize those later
		// Don't use current time, but instead use deformTimer, to
		// avoid changing the value of deformTimer.
		EmuTime::param time = deformTimer.getTime();
		for (int channel = 0; channel < 5; ++channel) {
			unsigned per = orgPeriod[channel];
			setFreqVol(2 * channel + 0, (per & 0x0FF) >> 0, time);
			setFreqVol(2 * channel + 1, (per & 0xF00) >> 8, time);
		}
	}

	// call to setFreqVol() modifies these variables, see above
	ar.serialize("count", count);
	ar.serialize("pos", pos);
	ar.serialize("out", out);
}
INSTANTIATE_SERIALIZE_METHODS(SCC);

} // namespace openmsx
