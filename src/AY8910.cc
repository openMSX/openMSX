// $Id$

/**
 *
 * Emulation of the AY-3-8910
 *
 * Code largly taken from xmame-0.37b16.1
 * 
 * Based on various code snippets by Ville Hallik, Michael Cuddy,
 * Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.
 *
 */


#include <cassert>
#include "AY8910.hh"
#include "Mixer.hh"


AY8910::AY8910(AY8910Interface &interf) : interface(interf)
{
	setVolume(21000);	// TODO find a good value and put it in config file
	int bufSize = Mixer::instance()->registerSound(this);
	buffer = new int[bufSize];
	reset();
}


AY8910::~AY8910()
{
	delete[] buffer;
}


void AY8910::reset()
{
	oldEnable = 0;
	random = 1;
	outputA = outputB = outputC = 0;
	outputN = 0xff;
	periodA = periodB = periodC = periodN = periodE = 0;
	countA  = countB  = countC  = countN  = countE  = 0;
	EmuTime dummy;
	for (int i=0; i<=15; i++) {
		wrtReg(i, 0, dummy);
	}
	setInternalMute(true);	// set muted 
}


byte AY8910::readRegister(byte reg, const EmuTime &time)
{
	assert (reg<=15);
	
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION))
			//input
			regs[reg] = interface.readA(time);
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION))
			//input
			regs[reg] = interface.readB(time);
		break;
	}
	return regs[reg];
}


void AY8910::writeRegister(byte reg, byte value, const EmuTime &time)
{
	assert (reg<=15);
	if ((reg<AY_PORTA) && (reg==AY_ESHAPE || regs[reg]!=value)) {
		// update the output buffer before changing the register
		Mixer::instance()->updateStream(time);
	}
	wrtReg(reg, value, time);
}
void AY8910::wrtReg(byte reg, byte value, const EmuTime &time)
{
	int old;
	regs[reg] = value;
	
	switch (reg) {
	// A note about the period of tones, noise and envelope: for speed reasons,
	// we count down from the period to 0, but careful studies of the chip
	// output prove that it instead counts up from 0 until the counter becomes
	// greater or equal to the period. This is an important difference when the
	// program is rapidly changing the period to modulate the sound.
	// To compensate for the difference, when the period is changed we adjust
	// our internal counter.
	// Also, note that period = 0 is the same as period = 1. This is mentioned
	// in the YM2203 data sheets. However, this does NOT apply to the Envelope
	// period. In that case, period = 0 is half as period = 1.
	case AY_ACOARSE:
		regs[AY_ACOARSE] &= 0x0f;
	case AY_AFINE:
		old = periodA;
		periodA = (regs[AY_AFINE] + 256 * regs[AY_ACOARSE]) * updateStep;
		if (periodA == 0) periodA = updateStep;
		countA += periodA - old;
		if (countA <= 0) countA = 1;
		break;
	case AY_BCOARSE:
		regs[AY_BCOARSE] &= 0x0f;
	case AY_BFINE:
		old = periodB;
		periodB = (regs[AY_BFINE] + 256 * regs[AY_BCOARSE]) * updateStep;
		if (periodB == 0) periodB = updateStep;
		countB += periodB - old;
		if (countB <= 0) countB = 1;
		break;
	case AY_CCOARSE:
		regs[AY_CCOARSE] &= 0x0f;
	case AY_CFINE:
		old = periodC;
		periodC = (regs[AY_CFINE] + 256 * regs[AY_CCOARSE]) * updateStep;
		if (periodC == 0) periodC = updateStep;
		countC += periodC - old;
		if (countC <= 0) countC = 1;
		break;
	case AY_NOISEPER:
		regs[AY_NOISEPER] &= 0x1f;
		old = periodN;
		periodN = regs[AY_NOISEPER] * updateStep;
		if (periodN == 0) periodN = updateStep;
		countN += periodN - old;
		if (countN <= 0) countN = 1;
		break;
	case AY_AVOL:
		regs[AY_AVOL] &= 0x1f;
		envelopeA = regs[AY_AVOL] & 0x10;
		volA = envelopeA ? volE : volTable[regs[AY_AVOL]];
		checkMute();
		break;
	case AY_BVOL:
		regs[AY_BVOL] &= 0x1f;
		envelopeB = regs[AY_BVOL] & 0x10;
		volB = envelopeB ? volE : volTable[regs[AY_BVOL]];
		checkMute();
		break;
	case AY_CVOL:
		regs[AY_CVOL] &= 0x1f;
		envelopeC = regs[AY_CVOL] & 0x10;
		volC = envelopeC ? volE : volTable[regs[AY_CVOL]];
		checkMute();
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		old = periodE;
		periodE = ((regs[AY_EFINE] + 256 * regs[AY_ECOARSE])) * (2*updateStep);
		if (periodE == 0) periodE = updateStep;
		countE += periodE - old;
		if (countE <= 0) countE = 1;
		break;
	case AY_ESHAPE:
		/* envelope shapes:  */
		/* C AtAlH           */
		/* 0 0 x x  \___     */
		/* 0 1 x x  /___     */
		/* 1 0 0 0  \\\\     */
		/* 1 0 0 1  \___     */
		/* 1 0 1 0  \/\/     */
		/* 1 0 1 1  \        */
		/* 1 1 0 0  ////     */
		/* 1 1 0 1  /        */
		/* 1 1 1 0  /\/\     */
		/* 1 1 1 1  /___     */
		regs[AY_ESHAPE] &= 0x0f;
		attack = (regs[AY_ESHAPE] & 0x04) ? 0x0f : 0x00;
		if ((regs[AY_ESHAPE] & 0x08) == 0) {
			// if Continue = 0, map the shape to the equivalent one which has Continue = 1
			hold = true;
			alternate = attack;
		} else {
			hold = regs[AY_ESHAPE] & 0x01;
			alternate = regs[AY_ESHAPE] & 0x02;
		}
		countE = periodE;
		countEnv = 0x0f;
		holding = false;
		volE = volTable[countEnv ^ attack];
		if (envelopeA) volA = volE;
		if (envelopeB) volB = volE;
		if (envelopeC) volC = volE;
		break;
	case AY_ENABLE:
		if ((value     & PORT_A_DIRECTION) &&
		   !(oldEnable & PORT_A_DIRECTION)) {
			// changed from input to output
			wrtReg(AY_PORTA, regs[AY_PORTA], time);
		}
		if ((value     & PORT_B_DIRECTION) &&
		   !(oldEnable & PORT_B_DIRECTION)) {
			// changed from input to output
			wrtReg(AY_PORTB, regs[AY_PORTB], time);
		}
		oldEnable = value;
		checkMute();
		break;
	case AY_PORTA:
		if (regs[AY_ENABLE] & PORT_A_DIRECTION)
			//output
			interface.writeA(value, time);
		break;
	case AY_PORTB:
		if (regs[AY_ENABLE] & PORT_B_DIRECTION)
			//output
			interface.writeB(value, time);
		break;
	}
}

void AY8910::checkMute()
{
	bool chA = (regs[AY_AVOL] == 0) || ((regs[AY_ENABLE]&0x09)==0x09);
	bool chB = (regs[AY_BVOL] == 0) || ((regs[AY_ENABLE]&0x12)==0x12);
	bool chC = (regs[AY_CVOL] == 0) || ((regs[AY_ENABLE]&0x24)==0x24);
	bool muted = chA && chB && chC;
	PRT_DEBUG("AY8910 muted: " << muted);
	setInternalMute(muted);
}

void AY8910::setInternalVolume(short newVolume)
{
	// calculate the volume->voltage conversion table
	// The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step)
	double out = newVolume;		// avoid clipping
	for (int i=15; i>0; i--) {
		volTable[i] = (unsigned int)(out+0.5);	// round to nearest
		out *= 0.707945784384;			// 1/(10^(3/20)) = 1/(3dB)
	}
	volTable[0] = 0;
}


void AY8910::setSampleRate (int sampleRate)
{
	// The step clock for the tone and noise generators is the chip clock
	// divided by 8; for the envelope generator of the AY-3-8910, it is half
	// that much (clock/16).
	// Here we calculate the number of steps which happen during one sample
	// at the given sample rate. No. of events = sample rate / (clock/8).
	// FP_UNIT is a multiplier used to turn the fraction into a fixed point
	// number.
	updateStep = ((FP_UNIT * sampleRate) / (CLOCK/8));	// !! look out for overflow !!
	PRT_DEBUG("UpdateStep "<<updateStep);
}


int* AY8910::updateBuffer(int length)
{
	// The 8910 has three outputs, each output is the mix of one of the three
	// tone generators and of the (single) noise generator. The two are mixed
	// BEFORE going into the DAC. The formula to mix each channel is:
	//    (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable).
	// Note that this means that if both tone and noise are disabled, the output
	// is 1, not 0, and can be modulated changing the volume.

	// If the channels are disabled, set their output to 1, and increase the
	// counter, if necessary, so they will not be inverted during this update.
	// Setting the output to 1 is necessary because a disabled channel is locked
	// into the ON state (see above); and it has no effect if the volume is 0.
	// If the volume is 0, increase the counter, but don't touch the output.

	PRT_DEBUG("AY8910 updateBuffer");

	if (regs[AY_ENABLE] & 0x01) {	// disabled
		if (countA <= length*FP_UNIT) countA += length*FP_UNIT;
		outputA = 1;
	} else if (regs[AY_AVOL] == 0) {
		// note that I do count += length, NOT count = length + 1. You might think
		// it's the same since the volume is 0, but doing the latter could cause
		// interferencies when the program is rapidly modulating the volume.
		if (countA <= length*FP_UNIT) countA += length*FP_UNIT;
	}
	if (regs[AY_ENABLE] & 0x02) {
		if (countB <= length*FP_UNIT) countB += length*FP_UNIT;
		outputB = 1;
	} else if (regs[AY_BVOL] == 0) {
		if (countB <= length*FP_UNIT) countB += length*FP_UNIT;
	}
	if (regs[AY_ENABLE] & 0x04) {
		if (countC <= length*FP_UNIT) countC += length*FP_UNIT;
		outputC = 1;
	} else if (regs[AY_CVOL] == 0) {
		if (countC <= length*FP_UNIT) countC += length*FP_UNIT;
	}

	// for the noise channel we must not touch outputN
	// it's also not necessary since we use outn.
	if ((regs[AY_ENABLE] & 0x38) == 0x38)	// all off
		if (countN <= length*FP_UNIT) countN += length*FP_UNIT;
	int outn = (outputN | regs[AY_ENABLE]);

	// buffering loop
	int* buf = buffer;
	while (length) {
		// semiVolA, semiVolB and semiVolC keep track of how long each
		// square wave stays in the 1 position during the sample period.
		int semiVolA = 0;
		int semiVolB = 0;
		int semiVolC = 0;
		int left = FP_UNIT;
		do {
			int nextevent = MIN (countN, left);
			if (outn & 0x08) {
				if (outputA) semiVolA += countA;
				countA -= nextevent;
				// periodA is the half period of the square wave. Here, in each
				// loop I add periodA twice, so that at the end of the loop the
				// square wave is in the same status (0 or 1) it was at the start.
				// semiVolA is also incremented by periodA, since the wave has been 1
				// exactly half of the time, regardless of the initial position.
				// If we exit the loop in the middle, outputA has to be inverted
				// and semiVolA incremented only if the exit status of the square
				// wave is 1.
				while (countA <= 0) {
					countA += periodA;
					if (countA > 0) {
						outputA ^= 1;
						if (outputA) semiVolA += periodA;
						break;
					}
					countA += periodA;
					semiVolA += periodA;
				}
				if (outputA) semiVolA -= countA;
			} else {
				countA -= nextevent;
				while (countA <= 0) {
					countA += periodA;
					if (countA > 0) {
						outputA ^= 1;
						break;
					}
					countA += periodA;
				}
			}

			if (outn & 0x10) {
				if (outputB) semiVolB += countB;
				countB -= nextevent;
				while (countB <= 0) {
					countB += periodB;
					if (countB > 0) {
						outputB ^= 1;
						if (outputB) semiVolB += periodB;
						break;
					}
					countB += periodB;
					semiVolB += periodB;
				}
				if (outputB) semiVolB -= countB;
			} else {
				countB -= nextevent;
				while (countB <= 0) {
					countB += periodB;
					if (countB > 0) {
						outputB ^= 1;
						break;
					}
					countB += periodB;
				}
			}

			if (outn & 0x20) {
				if (outputC) semiVolC += countC;
				countC -= nextevent;
				while (countC <= 0) {
					countC += periodC;
					if (countC > 0) {
						outputC ^= 1;
						if (outputC) semiVolC += periodC;
						break;
					}
					countC += periodC;
					semiVolC += periodC;
				}
				if (outputC) semiVolC -= countC;
			} else {
				countC -= nextevent;
				while (countC <= 0) {
					countC += periodC;
					if (countC > 0) {
						outputC ^= 1;
						break;
					}
					countC += periodC;
				}
			}

			countN -= nextevent;
			if (countN <= 0) {
				// Is noise output going to change?
				if ((random + 1) & 2) {	// bit0 ^ bit1
					outputN = ~outputN;
					outn = (outputN | regs[AY_ENABLE]);
				}
				// The Random Number Generator of the 8910 is a 17-bit shift
				// register. The input to the shift register is bit0 XOR bit2
				// (bit0 is the output).
				// The following is a fast way to compute bit 17 = bit0^bit2.
				// Instead of doing all the logic operations, we only check
				// bit 0, relying on the fact that after two shifts of the
				// register, what now is bit 2 will become bit 0, and will
				// invert, if necessary, bit 16, which previously was bit 18.
				if (random & 1) random ^= 0x28000;
				random >>= 1;
				countN += periodN;
			}

			left -= nextevent;
		} while (left > 0);

		/* update envelope */
		if (holding == false) {
			countE -= FP_UNIT;
			if (countE <= 0) {
				do {
					countEnv--;
					countE += periodE;
				} while (countE <= 0);

				/* check envelope current position */
				if (countEnv < 0) {
					if (hold) {
						if (alternate) attack ^= 0x0f;
						holding = true;
						countEnv = 0;
					} else {
						/* if countEnv has looped an odd number of times (usually 1), */
						/* invert the output. */
						if (alternate && (countEnv & 0x10)) attack ^= 0x0f;
						countEnv &= 0x0f;
					}
				}
				volE = volTable[countEnv ^ attack];
				/* reload volume */
				if (envelopeA) volA = volE;
				if (envelopeB) volB = volE;
				if (envelopeC) volC = volE;
			}
		}

		int chA = (semiVolA * volA) / FP_UNIT;
		int chB = (semiVolB * volB) / FP_UNIT;
		int chC = (semiVolC * volC) / FP_UNIT;
		*(buf++) = chA + chB + chC;
		length--;
	}
	return buffer;
}
