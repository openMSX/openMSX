// $Id$

/**
  * Based on:
  *    emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001
  * heavily rewritten to fit openMSX structure
  */

#include "YM2413.hh"

#if !defined(DONT_WANT_FMPAC) || !defined(DONT_WANT_MSXMUSIC)

#include "Mixer.hh"

#include <math.h>
#include <cassert>

word YM2413::fullsintable[PG_WIDTH];
word YM2413::halfsintable[PG_WIDTH];
int YM2413::dphaseNoiseTable[512][8];
word* YM2413::waveform[2] = {fullsintable, halfsintable};
int YM2413::pmtable[PM_PG_WIDTH];
int YM2413::amtable[AM_PG_WIDTH];
int YM2413::pm_dphase;
int YM2413::am_dphase;
word YM2413::AR_ADJUST_TABLE[1<<EG_BITS];
YM2413::Patch YM2413::nullPatch(false, false, false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
int YM2413::dphaseARTable[16][16];
int YM2413::dphaseDRTable[16][16];
int YM2413::tllTable[16][8][1<<TL_BITS][4];
int YM2413::rksTable[2][8][2];
int YM2413::dphaseTable[512][8][16];
short YM2413::dB2LinTab[(2*DB_MUTE)*2];


//***************************************************//
//                                                   //
//  Helper functions                                 //
//                                                   //
//***************************************************//

int YM2413::Slot::EG2DB(int d) 
{
	return d*(int)(EG_STEP/DB_STEP);
}
int YM2413::TL2EG(int d)
{ 
	return d*(int)(TL_STEP/EG_STEP);
}
int YM2413::Slot::SL2EG(int d)
{
	return d*(int)(SL_STEP/EG_STEP);
}

int YM2413::DB_POS(int x)
{
	return (int)(x/DB_STEP);
}
int YM2413::DB_NEG(int x)
{
	return (int)(2*DB_MUTE+x/DB_STEP);
}

// Cut the lower b bit off
int YM2413::HIGHBITS(int c, int b)
{
	return c >> b;
}
// Leave the lower b bit
int YM2413::LOWBITS(int c, int b)
{
	return c & ((1<<b)-1);
}
// Expand x which is s bits to d bits
int YM2413::EXPAND_BITS(int x, int s, int d)
{
	return x<<(d-s);
}
// Adjust envelope speed which depends on sampling rate
int YM2413::rate_adjust(double x, int rate)
{
	return (int)(x*CLOCK_FREQ/72/rate + 0.5); // +0.5 to round
}


//***************************************************//
//                                                   //
//                  Create tables                    //
//                                                   //
//***************************************************//

// Table for AR to LogCurve.
void YM2413::makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = (1<<EG_BITS);
	for (int i=1; i<128; i++)
		AR_ADJUST_TABLE[i] = (int)((double)(1<<EG_BITS) - 1 - (1<<EG_BITS) * log(i) / log(128)); 
}

// Table for dB(0 -- (1<<DB_BITS)) to Liner(0 -- DB2LIN_AMP_WIDTH)
void YM2413::makeDB2LinTable()
{
	for (int i=0; i<2*DB_MUTE; i++) {
		dB2LinTab[i] = (i<DB_MUTE) ? 
		        (int)((double)((1<<DB2LIN_AMP_BITS)-1)*pow(10,-(double)i*DB_STEP/20)) :
		        0;
		dB2LinTab[i+2*DB_MUTE] = -dB2LinTab[i];
	}
}

// Sin Table
void YM2413::makeSinTable()
{
	for (int i=0; i<PG_WIDTH/4; i++)
		fullsintable[i] = lin2db(sin(2.0*PI*i/PG_WIDTH));
	for (int i=0; i<PG_WIDTH/4; i++)
		fullsintable[PG_WIDTH/2 - 1 - i] = fullsintable[i];
	for (int i=0; i<PG_WIDTH/2; i++)
		fullsintable[PG_WIDTH/2+i] = 2*DB_MUTE + fullsintable[i];

	for (int i=0; i<PG_WIDTH/2; i++)
		halfsintable[i] = fullsintable[i];
	for (int i=PG_WIDTH/2; i<PG_WIDTH; i++)
		halfsintable[i] = fullsintable[0];
}
// Liner(+0.0 - +1.0) to dB((1<<DB_BITS) - 1 -- 0)
int YM2413::lin2db(double d)
{
	if (d == 0)
		return DB_MUTE-1;
	else
		return min(-(int)(20.0*log10(d)/DB_STEP), DB_MUTE-1);	// 0 -- 128
}
int YM2413::min(int i, int j) {
	if (i<j) return i; else return j;
}



void YM2413::makeDphaseNoiseTable(int sampleRate)
{
	for (int i=0; i<512; i++)
		for (int j=0; j<8; j++)
			dphaseNoiseTable[i][j] = rate_adjust(i<<j, sampleRate);
}

// Table for Pitch Modulator
void YM2413::makePmTable()
{
	for (int i=0; i<PM_PG_WIDTH; i++)
		pmtable[i] = (int)((double)PM_AMP * pow(2,(double)PM_DEPTH*sin(2.0*PI*i/PM_PG_WIDTH)/1200));
}

// Table for Amp Modulator
void YM2413::makeAmTable()
{
	for (int i=0; i<AM_PG_WIDTH; i++)
		amtable[i] = (int)((double)AM_DEPTH/2/DB_STEP * (1.0 + sin(2.0*PI*i/PM_PG_WIDTH)));
}

/* Phase increment counter table */ 
void YM2413::makeDphaseTable(int sampleRate)
{
	int mltable[16] = {
		1,1*2,2*2,3*2,4*2,5*2,6*2,7*2,8*2,9*2,10*2,10*2,12*2,12*2,15*2,15*2
	};

	for (int fnum=0; fnum<512; fnum++)
		for (int block=0; block<8; block++)
			for (int ML=0; ML<16; ML++)
				dphaseTable[fnum][block][ML] = 
					rate_adjust(((fnum * mltable[ML])<<block)>>(20-DP_BITS), sampleRate);
}

void YM2413::makeTllTable()
{
	#define dB2(x) (int)((x)*2)
	int kltable[16] = {
		dB2( 0.000),dB2( 9.000),dB2(12.000),dB2(13.875),
		dB2(15.000),dB2(16.125),dB2(16.875),dB2(17.625),
		dB2(18.000),dB2(18.750),dB2(19.125),dB2(19.500),
		dB2(19.875),dB2(20.250),dB2(20.625),dB2(21.000)
	};
  
	for (int fnum=0; fnum<16; fnum++) {
		for (int block=0; block<8; block++) {
			for (int TL=0; TL<64; TL++) {
				for (int KL=0; KL<4; KL++) {
					if (KL==0) {
						tllTable[fnum][block][TL][KL] = TL2EG(TL);
					} else {
						int tmp = kltable[fnum] - dB2(3.000) * (7 - block);
						if (tmp <= 0) {
							tllTable[fnum][block][TL][KL] = TL2EG(TL);
						} else { 
							tllTable[fnum][block][TL][KL] =
								(int)((tmp>>(3-KL))/EG_STEP) + TL2EG(TL);
						}
					}
				}
			}
		}
	}
}

// Rate Table for Attack
void YM2413::makeDphaseARTable(int sampleRate)
{
	for (int AR=0; AR<16; AR++) {
		for (int Rks=0; Rks<16; Rks++) {
			int RM = AR + (Rks>>2);
			if (RM>15) RM = 15;
			int RL = Rks&3;
			switch(AR) { 
			case 0:
				dphaseARTable[AR][Rks] = 0;
				break;
			case 15:
				dphaseARTable[AR][Rks] = EG_DP_WIDTH;
				break;
			default:
				dphaseARTable[AR][Rks] = rate_adjust(3*(RL+4) << (RM+1), sampleRate);
				break;
			}
		}
	}
}

// Rate Table for Decay
void YM2413::makeDphaseDRTable(int sampleRate)
{
	for (int DR=0; DR<16; DR++) {
		for (int Rks=0; Rks<16; Rks++) {
			int RM = DR + (Rks>>2);
			int RL = Rks&3;
			if (RM>15) RM = 15;
			switch(DR) { 
			case 0:
				dphaseDRTable[DR][Rks] = 0;
				break;
			default:
				dphaseDRTable[DR][Rks] = rate_adjust((RL+4) << (RM-1), sampleRate);
				break;
			}
		}
	}
}

void YM2413::makeRksTable()
{
	for (int fnum8=0; fnum8<2; fnum8++) {
		for (int block=0; block<8; block++) {
			for (int KR=0; KR<2; KR++) {
				rksTable[fnum8][block][KR] = (KR != 0) ?
					(block<<1) + fnum8:
					 block>>1;
			}
		}
	}
}

//************************************************************//
//                                                            //
//                      Patch                                 //
//                                                            //
//************************************************************//

YM2413::Patch::Patch(bool AM, bool PM, bool EG, byte KR, byte ML, byte KL,
                     byte TL, byte FB, byte WF, byte AR, byte DR, byte SL,
                     byte RR)
{
	this->AM = AM;
	this->PM = PM;
	this->EG = EG;
	this->KR = KR;
	this->ML = ML;
	this->KL = KL;
	this->TL = TL;
	this->FB = FB;
	this->WF = WF;
	this->AR = AR;
	this->DR = DR;
	this->SL = SL;
	this->RR = RR;
}

//************************************************************//
//                                                            //
//                      Slot                                  //
//                                                            //
//************************************************************//

// Constructor
YM2413::Slot::Slot(bool type)
{
	this->type = type;
}

// Destructor
YM2413::Slot::~Slot()
{
}

void YM2413::Slot::reset()
{
	sintbl = waveform[0];
	phase = 0;
	dphase = 0;
	output[0] = 0;
	output[1] = 0;
	feedback = 0;
	eg_mode = FINISH;
	eg_phase = EG_DP_WIDTH;
	eg_dphase = 0;
	rks = 0;
	tll = 0;
	sustine = 0;
	fnum = 0;
	block = 0;
	volume = 0;
	pgout = 0;
	egout = 0;
	patch = &nullPatch;
	slotStatus = false;
}


void YM2413::Slot::updatePG()
{
	dphase = dphaseTable[fnum][block][patch->ML];
}

void YM2413::Slot::updateTLL()
{
	tll = type ? tllTable[fnum>>5][block][volume]   [patch->KL]:
	             tllTable[fnum>>5][block][patch->TL][patch->KL];
}

void YM2413::Slot::updateRKS()
{
	rks = rksTable[fnum>>8][block][patch->KR];
}

void YM2413::Slot::updateWF()
{
	sintbl = waveform[patch->WF];
}

void YM2413::Slot::updateEG()
{
	switch(eg_mode) {
	case ATTACK:
		eg_dphase = dphaseARTable[patch->AR][rks];
		break;
	case DECAY:
		eg_dphase = dphaseDRTable[patch->DR][rks];
		break;
	case SUSTINE:
		eg_dphase = dphaseDRTable[patch->RR][rks];
		break;
	case RELEASE:
		if (sustine) 
			eg_dphase = dphaseDRTable[5][rks];
		else if (patch->EG)
			eg_dphase = dphaseDRTable[patch->RR][rks];
		else
			eg_dphase = dphaseDRTable[7][rks];
		break;
	case SUSHOLD:
	case FINISH:
		eg_dphase = 0;
		break;
	}
}

void YM2413::Slot::updateAll()
{
	updatePG();
	updateTLL();
	updateRKS();
	updateWF();
	updateEG(); // EG should be updated last
}


// Slot key on
void YM2413::Slot::slotOn()
{
	if (!slotStatus) {
		slotStatus = true;
		eg_mode = ATTACK;
		phase = 0;
		eg_phase = 0;
	}
}

// Slot key off
void YM2413::Slot::slotOff()
{
	if (slotStatus) {
		slotStatus = false;
		if (eg_mode == ATTACK)
			eg_phase = EXPAND_BITS(AR_ADJUST_TABLE[HIGHBITS(eg_phase,EG_DP_BITS-EG_BITS)],EG_BITS,EG_DP_BITS);
		eg_mode = RELEASE;
	}
}

// Change a rhythm voice
void YM2413::Slot::setPatch(Patch *ptch)
{
	patch = ptch;
}

void YM2413::Slot::setVolume(int newVolume)
{
	volume = newVolume;
}



//***********************************************************//
//                                                           //
//               Channel                                     //
//                                                           //
//***********************************************************//


// Constructor
YM2413::Channel::Channel() : mod(false), car(true)
{
	reset();
}

// Destructor
YM2413::Channel::~Channel()
{
}

// reset channel
void YM2413::Channel::reset()
{
	mod.reset();
	car.reset();
	setPatch(0);
}

// Change a voice
void YM2413::Channel::setPatch(int num)
{
	userPatch = (num == 0);
	mod.setPatch(&patches[2*num]);
	car.setPatch(&patches[2*num+1]);
}

// Set sustine parameter
void YM2413::Channel::setSustine(int sustine)
{
	car.sustine = sustine;
	if (mod.type)
		mod.sustine = sustine;
}

// Volume : 6bit ( Volume register << 2 )
void YM2413::Channel::setVol(int volume)
{
	car.volume = volume;
}

// Set F-Number (fnum : 9bit)
void YM2413::Channel::setFnumber(int fnum)
{
	car.fnum = fnum;
	mod.fnum = fnum;
}

// Set Block data (block : 3bit)
void YM2413::Channel::setBlock(int block)
{
	car.block = block;
	mod.block = block;
}

// Channel key on
void YM2413::Channel::keyOn()
{
	mod.slotOn();
	car.slotOn();
}

// Channel key off
void YM2413::Channel::keyOff()
{
	mod.slotOff();	////
	car.slotOff();
}



//***********************************************************//
//                                                           //
//               YM2413                                      //
//                                                           //
//***********************************************************//

// Constructor
YM2413::YM2413(short volume, const EmuTime &time, const Mixer::ChannelMode mode)
{
	// User instrument
	patches[ 0] = Patch(false, false, false, 0,  0, 0,  0, 0, 0,  0,  0,  0,  0);
	patches[ 1] = Patch(false, false, false, 0,  0, 0,  0, 0, 0,  0,  0,  0,  0);
	// Violin
	patches[ 2] = Patch(false,  true,  true, 0,  1, 0, 30, 7, 0, 15,  0,  0,  7);
	patches[ 3] = Patch(false,  true,  true, 0,  1, 0,  0, 0, 1,  7, 15,  1,  7);
	// Guitar
	patches[ 4] = Patch(false, false, false, 1,  3, 0, 15, 5, 1, 12, 14,  4,  3);
	patches[ 5] = Patch(false,  true, false, 0,  1, 0,  0, 0, 0, 15,  5,  2,  3);
	// Piano
	patches[ 6] = Patch(false, false, false, 0,  3, 2, 26, 4, 0, 15,  3,  1,  3);
	patches[ 7] = Patch(false, false, false, 0,  1, 0,  0, 0, 0, 15,  4,  2,  3);
	// Flute
	patches[ 8] = Patch(false, false,  true, 0,  1, 0, 29, 7, 0, 15, 10,  3,  0);
	patches[ 9] = Patch(false,  true,  true, 0,  1, 0,  0, 0, 0,  6,  4,  2,  8);
	// Clarinet
	patches[10] = Patch(false, false,  true, 0,  2, 0, 30, 6, 0, 15,  0,  1,  8);
	patches[11] = Patch(false, false,  true, 0,  1, 0,  0, 0, 0,  7,  6,  2,  8);
	// Oboe
	patches[12] = Patch(false, false,  true, 1,  1, 0, 22, 5, 0,  9,  0,  0,  0);
	patches[13] = Patch(false, false, false, 0,  2, 0,  0, 0, 0,  7,  1,  1,  0);
	// Trumpet
	patches[14] = Patch(false, false,  true, 0,  1, 0, 29, 7, 0,  8,  2,  1,  0);
	patches[15] = Patch(false,  true,  true, 0,  1, 0,  0, 0, 0,  8,  0,  1,  7);
	// Organ
	patches[16] = Patch(false, false,  true, 0,  3, 0, 45, 6, 0, 12,  0,  0,  7);
	patches[17] = Patch(false, false,  true, 0,  1, 0,  0, 0, 1,  7,  0,  0,  7);
	// Horn
	patches[18] = Patch(false,  true,  true, 0,  1, 0, 27, 6, 0,  6,  4,  1,  8);
	patches[19] = Patch(false, false,  true, 0,  1, 0,  0, 0, 0,  6,  5,  1,  8);
	// Synthesizer
	patches[20] = Patch(false,  true,  true, 0,  1, 0, 12, 0, 1,  8,  5,  7,  9);
	patches[21] = Patch(false,  true,  true, 0,  1, 0,  0, 0, 1, 10,  0,  0,  7);
	// Harpsichord
	patches[22] = Patch(false, false,  true, 0,  3, 2,  7, 1, 0, 15,  0,  0,  0);
	patches[23] = Patch(false, false,  true, 0,  1, 0,  0, 0, 1, 10,  4, 15,  7);
	// Vibraphone
	patches[24] = Patch( true, false, false, 1,  7, 0, 40, 7, 0, 15, 15,  0,  2);
	patches[25] = Patch( true,  true,  true, 0,  1, 0,  0, 0, 0, 15,  3, 15,  8);
	// Synthesizer bass
	patches[26] = Patch(false,  true,  true, 0,  1, 0, 12, 5, 0, 15,  2,  4,  0);
	patches[27] = Patch(false, false, false, 1,  0, 0,  0, 0, 0, 12,  4, 12,  8);
	// Acoustic bass
	patches[28] = Patch(false, false, false, 0,  1, 1, 22, 3, 0, 11,  4,  2,  3);
	patches[29] = Patch(false, false, false, 0,  1, 0,  0, 0, 0, 11,  2,  5,  8);
	// Electric guitar
	patches[30] = Patch(false,  true,  true, 0,  1, 2,  9, 3, 0, 15,  1, 15,  0);
	patches[31] = Patch(false,  true, false, 0,  1, 0,  0, 0, 0, 15,  4,  1,  3);
	
	patches[32] = Patch(false, false, false, 0,  4, 0, 22, 0, 0, 13, 15, 15, 15);
	patches[33] = Patch(false, false,  true, 0,  1, 0,  0, 0, 0, 15,  8, 15,  8);
	patches[34] = Patch(false, false,  true, 0,  3, 0,  0, 0, 0, 13,  8, 15,  8);
	patches[35] = Patch(false, false,  true, 1,  2, 0,  0, 0, 0, 15,  7, 15,  7);
	patches[36] = Patch(false, false,  true, 0,  5, 0,  0, 0, 0, 15,  8, 15,  8);
	patches[37] = Patch(false, false, false, 1,  8, 0,  0, 0, 0, 13, 10,  5,  5);

	for (int i=0; i<9; i++) {
		// TODO cleanup
		ch[i].mod.plfo_am = &lfo_am;
		ch[i].mod.plfo_pm = &lfo_pm;
		ch[i].car.plfo_am = &lfo_am;
		ch[i].car.plfo_pm = &lfo_pm;
		ch[i].patches = patches;
	}
	makePmTable();
	makeAmTable();
	makeAdjustTable();
	makeTllTable();
	makeRksTable();
	makeSinTable();
	makeDB2LinTable();

	reset(time);

	setVolume(volume);
	int bufSize = Mixer::instance()->registerSound(this,mode);
	buffer = new int[bufSize];
}

// Destructor
YM2413::~YM2413()
{
	Mixer::instance()->unregisterSound(this);
	delete[] buffer;
}

// Reset whole of OPLL except patch datas
void YM2413::reset(const EmuTime &time)
{
	output[0] = 0;
	output[1] = 0;
	pm_phase = 0;
	am_phase = 0;
	noise_seed =0xffff;
	noiseA = 0;
	noiseB = 0;
	noiseA_phase = 0;
	noiseB_phase = 0;
	noiseA_dphase = 0;
	noiseB_dphase = 0;
	for(int i=0; i<9; i++)
		ch[i].reset();
	for (int i=0; i<0x40; i++)
		writeReg(i, 0, time);	// optimization: pass time only once
	setInternalMute(true);	// set muted
}

void YM2413::setSampleRate(int sampleRate)
{
	makeDphaseTable(sampleRate);
	makeDphaseARTable(sampleRate);
	makeDphaseDRTable(sampleRate);
	makeDphaseNoiseTable(sampleRate);
	pm_dphase = rate_adjust(PM_SPEED * PM_DP_WIDTH / (CLOCK_FREQ/72), sampleRate);
	am_dphase = rate_adjust(AM_SPEED * AM_DP_WIDTH / (CLOCK_FREQ/72), sampleRate);
}


// Drum key on
void YM2413::keyOn_BD()  { ch[6].keyOn(); }
void YM2413::keyOn_HH()  { ch[7].mod.slotOn(); }
void YM2413::keyOn_SD()  { ch[7].car.slotOn(); }
void YM2413::keyOn_TOM() { ch[8].mod.slotOn(); }
void YM2413::keyOn_CYM() { ch[8].car.slotOn(); }

// Drum key off
void YM2413::keyOff_BD() { ch[6].keyOff(); }
void YM2413::keyOff_HH() { ch[7].mod.slotOff(); }
void YM2413::keyOff_SD() { ch[7].car.slotOff(); }
void YM2413::keyOff_TOM(){ ch[8].mod.slotOff(); }
void YM2413::keyOff_CYM(){ ch[8].car.slotOff(); }

// Change Rhythm Mode
void YM2413::setRythmMode(int data)
{
	bool newMode = (data & 32) != 0;
	if (rythm_mode != newMode) {
		rythm_mode = newMode;
		if (newMode) {
			// OFF->ON
			ch[6].setPatch(16);
			ch[7].setPatch(17);
			ch[8].setPatch(18);
			ch[7].mod.type = true;
			ch[8].mod.type = true;
		} else {
			// ON->OFF
			ch[6].setPatch(reg[0x36]>>4);
			ch[7].setPatch(reg[0x37]>>4);
			ch[8].setPatch(reg[0x38]>>4);
			ch[7].mod.type = false;
			ch[8].mod.type = false;
			
			ch[6].mod.eg_mode = FINISH; // BD1
			ch[6].mod.slotStatus = false;
			ch[6].car.eg_mode = FINISH; // BD2 
			ch[6].car.slotStatus = false;
			ch[7].mod.eg_mode = FINISH; // HH
			ch[7].mod.slotStatus = false;
			ch[7].car.eg_mode = FINISH; // SD
			ch[7].car.slotStatus = false;
			ch[8].mod.eg_mode = FINISH; // TOM
			ch[8].mod.slotStatus = false;
			ch[8].car.eg_mode = FINISH; // CYM
			ch[8].car.slotStatus = false;
		}
	}
}



//******************************************************//
//                                                      //
//                 Generate wave data                   //
//                                                      //
//******************************************************//

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 4PI)
int YM2413::Slot::wave2_4pi(int e) 
{
	int shift =  SLOT_AMP_BITS - PG_BITS - 1;
	if (shift > 0)
		return e >> shift;
	else
		return e << -shift;
}

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 8PI)
int YM2413::Slot::wave2_8pi(int e) 
{
	int shift = SLOT_AMP_BITS - PG_BITS - 2;
	if (shift > 0)
		return e >> shift;
	else
		return e << -shift;
}

// Update Noise unit
inline void YM2413::update_noise()
{
	if (noise_seed & 1)
		noise_seed ^= 0x24000;
	noise_seed >>= 1;
	whitenoise = noise_seed&1 ? DB_POS(6) : DB_NEG(6);

	noiseA_phase += noiseA_dphase;
	noiseB_phase += noiseB_dphase;

	noiseA_phase &= (0x40<<11) - 1;
	if ((noiseA_phase>>11)==0x3f)
		noiseA_phase = 0;
	noiseA = noiseA_phase&(0x03<<11)?DB_POS(6):DB_NEG(6);

	noiseB_phase &= (0x10<<11) - 1;
	noiseB = noiseB_phase&(0x0A<<11)?DB_POS(6):DB_NEG(6);
}

// Update AM, PM unit
inline void YM2413::update_ampm()
{
	pm_phase = (pm_phase + pm_dphase)&(PM_DP_WIDTH - 1);
	am_phase = (am_phase + am_dphase)&(AM_DP_WIDTH - 1);
	lfo_am = amtable[HIGHBITS(am_phase, AM_DP_BITS - AM_PG_BITS)];
	lfo_pm = pmtable[HIGHBITS(pm_phase, PM_DP_BITS - PM_PG_BITS)];
}

// PG
void YM2413::Slot::calc_phase()
{
	if (patch->PM) {
		phase += (dphase * (*plfo_pm)) >> PM_AMP_BITS;
	} else {
		phase += dphase;
	}
	phase &= (DP_WIDTH - 1);
	pgout = HIGHBITS(phase, DP_BASE_BITS);
}

// EG
void YM2413::Slot::calc_envelope()
{
	#define S2E(x) (SL2EG((int)(x/SL_STEP))<<(EG_DP_BITS-EG_BITS)) 
	int SL[16] = {
		S2E( 0), S2E( 3), S2E( 6), S2E( 9), S2E(12), S2E(15), S2E(18), S2E(21),
		S2E(24), S2E(27), S2E(30), S2E(33), S2E(36), S2E(39), S2E(42), S2E(48)
	};

	switch(eg_mode) {
	case ATTACK:
		eg_phase += eg_dphase;
		if (EG_DP_WIDTH & eg_phase) {
			egout = 0;
			eg_phase= 0;
			eg_mode = DECAY;
			updateEG();
		} else {
			egout = AR_ADJUST_TABLE[HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS)];
		}
		break;
	case DECAY:
		eg_phase += eg_dphase;
		egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		if (eg_phase >= SL[patch->SL]) {
			if (patch->EG) {
				eg_phase = SL[patch->SL];
				eg_mode = SUSHOLD;
				updateEG();
			} else {
				eg_phase = SL[patch->SL];
				eg_mode = SUSTINE;
				updateEG();
			}
			egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		}
		break;
	case SUSHOLD:
		egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		if (!patch->EG) {
			eg_mode = SUSTINE;
			updateEG();
		}
		break;
	case SUSTINE:
	case RELEASE:
		eg_phase += eg_dphase;
		egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		if (egout >= (1<<EG_BITS)) {
			eg_mode = FINISH;
			egout = (1<<EG_BITS) - 1;
		}
		break;
	case FINISH:
	default:
		egout = (1<<EG_BITS) - 1;
		break;
	}
	if (patch->AM) 
		egout = EG2DB(egout+tll) + (*plfo_am);
	else 
		egout = EG2DB(egout+tll);
	if (egout >= DB_MUTE)
		egout = DB_MUTE-1;
}

// CARRIOR
int YM2413::Slot::calc_slot_car(int fm)
{
	calc_envelope();
	calc_phase();
	output[1] = output[0];

	if (egout>=(DB_MUTE-1)) {
		output[0] = 0;
	} else {
		output[0] = dB2LinTab[sintbl[(pgout+wave2_8pi(fm))&(PG_WIDTH-1)] + egout];
	}
	return (output[1] + output[0]) >> 1;
}

// MODULATOR
int YM2413::Slot::calc_slot_mod()
{
	output[1] = output[0];
	calc_envelope();
	calc_phase();

	if(egout>=(DB_MUTE-1)) {
		output[0] = 0;
	} else if (patch->FB != 0) {
		int fm = wave2_4pi(feedback) >> (7 - patch->FB);
		output[0] = dB2LinTab[sintbl[(pgout+fm)&(PG_WIDTH-1)] + egout];
	} else {
		output[0] = dB2LinTab[sintbl[pgout] + egout];
	}
	feedback = (output[1] + output[0]) >> 1;
	return feedback;
}

// TOM
int YM2413::Slot::calc_slot_tom()
{
	calc_envelope(); 
	calc_phase();
	if (egout>=(DB_MUTE-1))
		return 0;
	return dB2LinTab[sintbl[pgout] + egout];
}

// SNARE
int YM2413::Slot::calc_slot_snare(int whitenoise)
{
	calc_envelope();
	calc_phase();
	if (egout>=(DB_MUTE-1))
		return 0;
	if (pgout & (1<<(PG_BITS-1))) {
		return (dB2LinTab[egout] + dB2LinTab[egout+whitenoise]) >> 1;
	} else {
		return (dB2LinTab[2*DB_MUTE + egout] + dB2LinTab[egout+whitenoise]) >> 1;
	}
}

// TOP-CYM
int YM2413::Slot::calc_slot_cym(int a, int b)
{
	calc_envelope();
	if (egout>=(DB_MUTE-1)) {
		return 0;
	} else {
		return (dB2LinTab[egout+a] + dB2LinTab[egout+b]) >> 1;
	}
}

// HI-HAT
int YM2413::Slot::calc_slot_hat(int a, int b, int whitenoise)
{
	calc_envelope();
	if (egout>=(DB_MUTE-1)) {
		return 0;
	} else {
		return (dB2LinTab[egout+whitenoise] + dB2LinTab[egout+a] + dB2LinTab[egout+b]) >>2;
	}
}

/** @param channelMask Bit n is 1 iff channel n is enabled.
  */
inline int YM2413::calcSample(int channelMask)
{
	// while muted updated_ampm() and update_noise() aren't called, probably ok
	update_ampm();
	update_noise();

	int mix = 0;

	if (rythm_mode) {
		ch[7].mod.calc_phase();
		ch[8].car.calc_phase();

		if (channelMask & (1 << 6))
			mix += ch[6].car.calc_slot_car(ch[6].mod.calc_slot_mod());
		if (ch[7].mod.eg_mode != FINISH)
			mix += ch[7].mod.calc_slot_hat(noiseA, noiseB, whitenoise);
		if (channelMask & (1 << 7))
			mix += ch[7].car.calc_slot_snare(whitenoise);
		if (ch[8].mod.eg_mode != FINISH)
			mix += ch[8].mod.calc_slot_tom();
		if (channelMask & (1 << 8))
			mix += ch[8].car.calc_slot_cym(noiseA, noiseB);

		channelMask &= (1 << 6) - 1;
		mix *= 2;
	}

	for (Channel *cp = ch; channelMask; channelMask >>= 1, cp++) {
		if (channelMask & 1)
			mix += cp->car.calc_slot_car(cp->mod.calc_slot_mod());
	}

	return (maxVolume*mix)>>DB2LIN_AMP_BITS;
}

void YM2413::checkMute()
{
	setInternalMute(checkMuteHelper());
}
bool YM2413::checkMuteHelper()
{
	for (int i=0; i<6; i++) {
		if (ch[i].car.eg_mode!=FINISH) return false;
	}
	if (!rythm_mode) {
		for(int i=6; i<9; i++) {
			 if (ch[i].car.eg_mode!=FINISH) return false;
		}
	} else {
		if (ch[6].car.eg_mode!=FINISH) return false;
		if (ch[7].mod.eg_mode!=FINISH) return false;
		if (ch[7].car.eg_mode!=FINISH) return false;
		if (ch[8].mod.eg_mode!=FINISH) return false;
		if (ch[8].car.eg_mode!=FINISH) return false;
	}
	return true;	// nothing is playing, then mute
}

int* YM2413::updateBuffer(int length)
{
	PRT_DEBUG("YM2413: update buffer");
	if (isInternalMuted()) {
		PRT_DEBUG("YM2413: muted");
		return NULL;
	}

	int channelMask = 0;
	for (int i = 9; i--; ) {
		channelMask <<= 1;
		if (ch[i].car.eg_mode != FINISH) channelMask |= 1;
	}

	int* buf = buffer;
	while (length--) {
		*(buf++) = calcSample(channelMask);
	}

	checkMute();
	return buffer;
}

void YM2413::setInternalVolume(short newVolume)
{
	maxVolume = newVolume;
}


//**************************************************//
//                                                  //
//                       I/O Ctrl                   //
//                                                  //
//**************************************************//

void YM2413::writeReg(byte regis, byte data, const EmuTime &time)
{
	PRT_DEBUG("YM2413: write reg "<<(int)regis<<" "<<(int)data);

	// update the output buffer before changing the register
	Mixer::instance()->updateStream(time);
	Mixer::instance()->lock();

	assert (regis < 0x40);
	switch (regis) {
	case 0x00:
		patches[0].AM = (data>>7)&1;
		patches[0].PM = (data>>6)&1;
		patches[0].EG = (data>>5)&1;
		patches[0].KR = (data>>4)&1;
		patches[0].ML = (data)&15;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].mod.updatePG();
				ch[i].mod.updateRKS();
				ch[i].mod.updateEG();
			}
		}
		break;
	case 0x01:
		patches[1].AM = (data>>7)&1;
		patches[1].PM = (data>>6)&1;
		patches[1].EG = (data>>5)&1;
		patches[1].KR = (data>>4)&1;
		patches[1].ML = (data)&15;
		for (int i=0; i<9; i++) {
			if(ch[i].userPatch) {
				ch[i].car.updatePG();
				ch[i].car.updateRKS();
				ch[i].car.updateEG();
			}
		}
		break;
	case 0x02:
		patches[0].KL = (data>>6)&3;
		patches[0].TL = (data)&63;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].mod.updateTLL();
			}
		}
		break;
	case 0x03:
		patches[1].KL = (data>>6)&3;
		patches[1].WF = (data>>4)&1;
		patches[0].WF = (data>>3)&1;
		patches[0].FB = (data)&7;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].mod.updateWF();
				ch[i].car.updateWF();
			}
		}
		break;
	case 0x04:
		patches[0].AR = (data>>4)&15;
		patches[0].DR = (data)&15;
		for (int i=0; i<9; i++) {
			if(ch[i].userPatch) {
				ch[i].mod.updateEG();
			}
		}
		break;
	case 0x05:
		patches[1].AR = (data>>4)&15;
		patches[1].DR = (data)&15;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].car.updateEG();
			}
		}
		break;
	case 0x06:
		patches[0].SL = (data>>4)&15;
		patches[0].RR = (data)&15;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].mod.updateEG();
			}
		}
		break;
	case 0x07:
		patches[1].SL = (data>>4)&15;
		patches[1].RR = (data)&15;
		for (int i=0; i<9; i++) {
			if (ch[i].userPatch) {
				ch[i].car.updateEG();
			}
		}
		break;
	case 0x0e:
		setRythmMode(data);
		if (rythm_mode) {
			if (data&0x10) keyOn_BD();  else keyOff_BD();
			if (data&0x08) keyOn_SD();  else keyOff_SD();
			if (data&0x04) keyOn_TOM(); else keyOff_TOM();
			if (data&0x02) keyOn_CYM(); else keyOff_CYM();
			if (data&0x01) keyOn_HH();  else keyOff_HH();
		}
		ch[6].mod.updateAll();
		ch[6].car.updateAll();
		ch[7].mod.updateAll();
		ch[7].car.updateAll();
		ch[8].mod.updateAll();
		ch[8].car.updateAll();        
		break;
	case 0x0f:
		break;
	case 0x10:  case 0x11:  case 0x12:  case 0x13:
	case 0x14:  case 0x15:  case 0x16:  case 0x17:
	case 0x18:
	{
		int cha = regis & 0x0f;
		int fNum = data + ((reg[0x20+cha]&1)<<8);
		int block = (reg[0x20+cha]>>1)&7;
		ch[cha].setFnumber(fNum);
		switch (cha) {
			case 7: noiseA_dphase = dphaseNoiseTable[fNum][block];
				break;
			case 8: noiseB_dphase = dphaseNoiseTable[fNum][block];
				break;
		}
		ch[cha].mod.updateAll();
		ch[cha].car.updateAll();
		break;
	}
	case 0x20:  case 0x21:  case 0x22:  case 0x23:
	case 0x24:  case 0x25:  case 0x26:  case 0x27:
	case 0x28:
	{
		int cha = regis & 0x0f;
		int fNum = ((data&1)<<8) + reg[0x10+cha];
		int block = (data>>1)&7;
		ch[cha].setFnumber(fNum);
		ch[cha].setBlock(block);
		switch (cha) {
			case 7: noiseA_dphase = dphaseNoiseTable[fNum][block];
				break;
			case 8: noiseB_dphase = dphaseNoiseTable[fNum][block];
				break;
		}
		ch[cha].setSustine((data>>5)&1);
		if (data&0x10) 
			ch[cha].keyOn();
		else
			ch[cha].keyOff();
		ch[cha].mod.updateAll();
		ch[cha].car.updateAll();
		break;
	}
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37: case 0x38: 
	{
		int cha = regis & 0x0f;
		int j = (data>>4)&15;
		int v = data&15;
		if ((rythm_mode)&&(regis>=0x36)) {
			switch(regis) {
			case 0x37:
				ch[7].mod.setVolume(j<<2);
				break;
			case 0x38:
				ch[8].mod.setVolume(j<<2);
				break;
			}
		} else { 
			ch[cha].setPatch(j);
		}
		ch[cha].setVol(v<<2);
		ch[cha].mod.updateAll();
		ch[cha].car.updateAll();
		break;
	}
	default:
		break;
	}
	reg[regis] = data;
	Mixer::instance()->unlock();
	checkMute();
}

#endif // not defined(DONT_WANT_FMPAC) || not defined(DONT_WANT_MSXMUSIC)
