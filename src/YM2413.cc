// $Id$
//**************************************************************************************//
//                                                                                      //
//  emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001                      //
//                                                                                      //
//  2001 01-08 : Version 0.10 -- 1st version.                                           //
//  2001 01-15 : Version 0.20 -- semi-public version.                                   //
//  2001 01-16 : Version 0.30 -- 1st public version.                                    //
//  2001 01-17 : Version 0.31 -- Fixed bassdrum problem.                                //
//             : Version 0.32 -- LPF implemented.                                       //
//  2001 01-18 : Version 0.33 -- Fixed the drum problem, refine the mix-down method.    //
//                            -- Fixed the LFO bug.                                     //
//  2001 01-24 : Version 0.35 -- Fixed the drum problem,                                //
//                               support undocumented EG behavior.                      //
//  2001 02-02 : Version 0.38 -- Improved the performance.                              //
//                               Fixed the hi-hat and cymbal model.                     //
//                               Fixed the default percussive datas.                    //
//                               Noise reduction.                                       //
//                               Fixed the feedback problem.                            //
//  2001 03-03 : Version 0.39 -- Fixed some drum bugs.                                  //
//                               Improved the performance.                              //
//  2001 03-04 : Version 0.40 -- Improved the feedback.                                 //
//                               Change the default table size.                         //
//                               Clock and Rate can be changed during play.             //
//  2001 06-24 : Version 0.50 -- Improved the hi-hat and the cymbal tone.               //
//                               Added VRC7 patch (OPLL_reset_patch is changed).        //
//                               Fixed OPLL_reset() bug.                                //
//                               Added OPLL_setMask, OPLL_getMask and OPLL_toggleMask.  //
//                               Added OPLL_writeIO.                                    //
//  2001 09-28 : Version 0.51 -- Remove the noise table.                                //
//                                                                                      //
//  References:                                                                         //
//    fmopl.c        -- 1999,2000 written by Tatsuyuki Satoh (MAME development).        //
//    s_opl.c        -- 2001 written by Mamiya (NEZplug development).                   //
//    fmgen.cpp      -- 1999,2000 written by cisc.                                      //
//    fmpac.ill      -- 2000 created by NARUTO.                                         //
//    MSX-Datapack                                                                      //
//    YMU757 data sheet                                                                 //
//    YM2143 data sheet                                                                 //
//                                                                                      //
//**************************************************************************************//

#include <math.h>
#include <cassert>
#include "YM2413.hh"

int YM2413::Slot::EG2DB(int d) { return d*(int)(EG_STEP/DB_STEP); }
int YM2413::TL2EG(int d) { return d*(int)(TL_STEP/EG_STEP); }
int YM2413::Slot::SL2EG(int d) { return d*(int)(SL_STEP/EG_STEP); }

int YM2413::DB_POS(int x) { return (int)(x/DB_STEP); }
int YM2413::DB_NEG(int x) { return (int)(DB_MUTE+DB_MUTE+x/DB_STEP); }

// Cut the lower b bit(s) off
int YM2413::HIGHBITS(int c, int b) { return c>>b; }
// Leave the lower b bit(s)
int YM2413::LOWBITS(int c, int b) { return c & ((1<<b)-1); }
// Expand x which is s bits to d bits
int YM2413::EXPAND_BITS(int x, int s, int d) { return x<<(d-s); }

// Adjust envelope speed which depends on sampling rate
int YM2413::rate_adjust(int x, int rate) { return (int)((double)x*CLOCK_FREQ/72/rate + 0.5); } // +0.5 to round


bool YM2413::alreadyInitialized = false;
word YM2413::fullsintable[PG_WIDTH];
word YM2413::halfsintable[PG_WIDTH];
int YM2413::dphaseNoiseTable[512][8];
word* YM2413::waveform[2] = {fullsintable, halfsintable};
int YM2413::pmtable[PM_PG_WIDTH];
int YM2413::amtable[AM_PG_WIDTH];
int YM2413::pm_dphase;
int YM2413::am_dphase;
word YM2413::AR_ADJUST_TABLE[1<<EG_BITS];
YM2413::Patch YM2413::null_patch;
YM2413::Patch YM2413::default_patch[(16+3)*2];
int YM2413::dphaseARTable[16][16];
int YM2413::dphaseDRTable[16][16];
int YM2413::tllTable[16][8][1<<TL_BITS][4];
int YM2413::rksTable[2][8][2];
int YM2413::dphaseTable[512][8][16];



//***************************************************//
//                                                   //
//                  Create tables                    //
//                                                   //
//***************************************************//

YM2413::Patch::Patch()
{
	TL = FB = EG = ML = AR = DR = SL = RR = KR = KL = AM = PM = WF = 0;
}



// Table for AR to LogCurve.
void YM2413::makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = (1<<EG_BITS);
	for (int i=1; i<128; i++)
		AR_ADJUST_TABLE[i] = (int)((double)(1<<EG_BITS) - 1 - (1<<EG_BITS) * log(i) / log(128)); 
}

// Table for dB(0 -- (1<<DB_BITS)) to Liner(0 -- DB2LIN_AMP_WIDTH)
void YM2413::setInternalVolume(short maxVolume)
{
	for(int i=0; i<DB_MUTE+DB_MUTE; i++) {
		dB2LinTab[i] = (i<DB_MUTE) ? 
		        (int)((double)maxVolume*pow(10,-(double)i*DB_STEP/20)) :
		        0;
		dB2LinTab[i+ DB_MUTE + DB_MUTE] = -dB2LinTab[i] ;
	}
}

// Sin Table
void YM2413::makeSinTable()
{
	for(int i=0; i<PG_WIDTH/4; i++)
		fullsintable[i] = lin2db(sin(2.0*PI*i/PG_WIDTH));
	for(int i=0; i<PG_WIDTH/4; i++)
		fullsintable[PG_WIDTH/2 - 1 - i] = fullsintable[i];
	for(int i=0; i<PG_WIDTH/2; i++)
		fullsintable[PG_WIDTH/2+i] = DB_MUTE + DB_MUTE + fullsintable[i];
  
	for(int i=0; i<PG_WIDTH/2; i++)
		halfsintable[i] = fullsintable[i];
	for(int i=PG_WIDTH/2 ; i<PG_WIDTH; i++)
		halfsintable[i] = fullsintable[0];
}
// Liner(+0.0 - +1.0) to dB((1<<DB_BITS) - 1 -- 0)
int YM2413::lin2db(double d)
{
	if (d == 0)
		return (DB_MUTE - 1);
	else
		return min(-(int)(20.0*log10(d)/DB_STEP), DB_MUTE - 1);	// 0 -- 128
}
int YM2413::min(int i, int j) {
	if (i<j) return i; else return j;
}



void YM2413::makeDphaseNoiseTable(int sampleRate)
{
	for(int i=0; i<512; i++)
		for(int j=0; j<8; j++)
			dphaseNoiseTable[i][j] = rate_adjust(i<<j, sampleRate);
}

// Table for Pitch Modulator
void YM2413::makePmTable()
{
	for(int i=0; i<PM_PG_WIDTH; i++)
		pmtable[i] = (int)((double)PM_AMP * pow(2,(double)PM_DEPTH*sin(2.0*PI*i/PM_PG_WIDTH)/1200));
}

// Table for Amp Modulator
void YM2413::makeAmTable()
{
	for(int i=0; i<AM_PG_WIDTH; i++)
		amtable[i] = (int)((double)AM_DEPTH/2/DB_STEP * (1.0 + sin(2.0*PI*i/PM_PG_WIDTH)));
}

/* Phase increment counter table */ 
void YM2413::makeDphaseTable(int sampleRate)
{
	int mltable[16] = {
		1,1*2,2*2,3*2,4*2,5*2,6*2,7*2,8*2,9*2,10*2,10*2,12*2,12*2,15*2,15*2
	};

	for(int fnum=0; fnum<512; fnum++)
		for(int block=0; block<8; block++)
			for(int ML=0; ML<16; ML++)
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
  
	for(int fnum=0; fnum<16; fnum++) {
		for(int block=0; block<8; block++) {
			for(int TL=0; TL<64; TL++) {
				for(int KL=0; KL<4; KL++) {
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
	for(int AR=0; AR<16; AR++) {
		for(int Rks=0; Rks<16; Rks++) {
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
				break ;
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
			if (RM>15) RM = 15 ;
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
	for(int fnum8=0; fnum8<2; fnum8++) {
		for(int block=0; block<8; block++) {
			for(int KR=0; KR<2; KR++) {
				if (KR!=0) {
					rksTable[fnum8][block][KR] = ( block << 1 ) + fnum8;
				} else {
					rksTable[fnum8][block][KR] = block >> 1;
				}
			}
		}
	}
}


void YM2413::makeDefaultPatch()
{
	for (int j=0; j<19; j++)
		getDefaultPatch(j, &default_patch[j*2]);
}
void YM2413::getDefaultPatch(int num, Patch *patch)
{
	dump2patch(default_inst + num*16, patch);
}
void YM2413::dump2patch(const byte *dump, Patch *patch)
{
	patch[0].AM = (dump[0]>>7)&1;
	patch[1].AM = (dump[1]>>7)&1;
	patch[0].PM = (dump[0]>>6)&1;
	patch[1].PM = (dump[1]>>6)&1;
	patch[0].EG = (dump[0]>>5)&1;
	patch[1].EG = (dump[1]>>5)&1;
	patch[0].KR = (dump[0]>>4)&1;
	patch[1].KR = (dump[1]>>4)&1;
	patch[0].ML = (dump[0])&15;
	patch[1].ML = (dump[1])&15;
	patch[0].KL = (dump[2]>>6)&3;
	patch[1].KL = (dump[3]>>6)&3;
	patch[0].TL = (dump[2])&63;
	patch[0].FB = (dump[3])&7;
	patch[0].WF = (dump[3]>>3)&1;
	patch[1].WF = (dump[3]>>4)&1;
	patch[0].AR = (dump[4]>>4)&15;
	patch[1].AR = (dump[5]>>4)&15;
	patch[0].DR = (dump[4])&15;
	patch[1].DR = (dump[5])&15;
	patch[0].SL = (dump[6]>>4)&15;
	patch[1].SL = (dump[7]>>4)&15;
	patch[0].RR = (dump[6])&15;
	patch[1].RR = (dump[7])&15;
}
byte YM2413::default_inst[(16+3)*16] = {
/* YM2413 VOICE */
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x61,0x61,0x1e,0x17,0xf0,0x7f,0x07,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x13,0x41,0x0f,0x0d,0xce,0xf5,0x43,0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x03,0x01,0x9a,0x04,0xf3,0xf4,0x13,0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x21,0x61,0x1d,0x07,0xfa,0x64,0x30,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x22,0x21,0x1e,0x06,0xf0,0x76,0x18,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x31,0x02,0x16,0x05,0x90,0x71,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x21,0x61,0x1d,0x07,0x82,0x80,0x10,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x23,0x21,0x2d,0x16,0xc0,0x70,0x07,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x61,0x21,0x1b,0x06,0x64,0x65,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x61,0x61,0x0c,0x18,0x85,0xa0,0x79,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x23,0x21,0x87,0x11,0xf0,0xa4,0x00,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x97,0xe1,0x28,0x07,0xff,0xf3,0x02,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x61,0x10,0x0c,0x05,0xf2,0xc4,0x40,0xc8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x01,0x56,0x03,0xb4,0xb2,0x23,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x61,0x41,0x89,0x03,0xf1,0xf4,0xf0,0x13,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x04,0x21,0x16,0x00,0xdf,0xf8,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x23,0x32,0x00,0x00,0xd8,0xf7,0xf8,0xf7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x25,0x18,0x00,0x00,0xf8,0xda,0xf8,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


//**********************************************************//
//                                                          //
//                      Calc Parameters                     //
//                                                          //
//**********************************************************//

int YM2413::Slot::calc_eg_dphase()
{
	switch(eg_mode) {
	case ATTACK:
		return dphaseARTable[patch->AR][rks];
	case DECAY:
		return dphaseDRTable[patch->DR][rks];
	case SUSHOLD:
		return 0;
	case SUSTINE:
		return dphaseDRTable[patch->RR][rks];
	case RELEASE:
		if (sustine)
			return dphaseDRTable[5][rks];
		else if (patch->EG)
			return dphaseDRTable[patch->RR][rks];
		else 
			return dphaseDRTable[7][rks];
	case FINISH:
		return 0;
	default:
		return 0;
	}
}

//************************************************************//
//                                                            //
//                    OPLL internal interfaces                //
//                                                            //
//************************************************************//

void YM2413::Slot::updatePG()
{
	dphase = dphaseTable[fnum][block][patch->ML];
}

void YM2413::Slot::updateTLL()
{
	(type==0) ? (tll = tllTable[fnum>>5][block][patch->TL][patch->KL]):
	            (tll = tllTable[fnum>>5][block][volume][patch->KL]);
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
	eg_dphase = calc_eg_dphase();
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
	eg_mode = ATTACK;
	phase = 0;
	eg_phase = 0;
}

// Slot key off
void YM2413::Slot::slotOff()
{
	if (eg_mode == ATTACK)
		eg_phase = EXPAND_BITS(AR_ADJUST_TABLE[HIGHBITS(eg_phase,EG_DP_BITS-EG_BITS)],EG_BITS,EG_DP_BITS);
	eg_mode = RELEASE;
}

// Channel key on
void YM2413::keyOn(int i)
{
	if (!slot_on_flag[i*2]) 
		ch[i]->mod->slotOn();
	if (!slot_on_flag[i*2+1]) 
		ch[i]->car->slotOn();
	ch[i]->key_status = true;
}

// Channel key off
void YM2413::keyOff(int i)
{
	if (slot_on_flag[i*2+1])
		ch[i]->car->slotOff();
	ch[i]->key_status = false;
}

// Drum key on
void YM2413::keyOn_BD()  { keyOn(6); }
void YM2413::keyOn_SD()  { if(!slot_on_flag[SLOT_SD])  ch[7]->car->slotOn(); }
void YM2413::keyOn_TOM() { if(!slot_on_flag[SLOT_TOM]) ch[8]->mod->slotOn(); }
void YM2413::keyOn_HH()  { if(!slot_on_flag[SLOT_HH])  ch[7]->mod->slotOn(); }
void YM2413::keyOn_CYM() { if(!slot_on_flag[SLOT_CYM]) ch[8]->car->slotOn(); }

// Drum key off
void YM2413::keyOff_BD() { keyOff(6); }
void YM2413::keyOff_SD() { if (slot_on_flag[SLOT_SD])  ch[7]->car->slotOff(); }
void YM2413::keyOff_TOM(){ if (slot_on_flag[SLOT_TOM]) ch[8]->mod->slotOff(); }
void YM2413::keyOff_HH() { if (slot_on_flag[SLOT_HH])  ch[7]->mod->slotOff(); }
void YM2413::keyOff_CYM(){ if (slot_on_flag[SLOT_CYM]) ch[8]->car->slotOff(); }

// Change a voice
void YM2413::setPatch(int i, int num)
{
	ch[i]->patch_number = num;
	ch[i]->mod->patch = patch[num*2+0];
	ch[i]->car->patch = patch[num*2+1];
}

// Change a rhythm voice
void YM2413::Slot::setSlotPatch(Patch *patch)
{
	this->patch = patch;
}

// Set sustine parameter
void YM2413::setSustine(int c, int sustine)
{
	ch[c]->car->sustine = sustine;
	if (ch[c]->mod->type)
		ch[c]->mod->sustine = sustine;
}

// Volume : 6bit ( Volume register << 2 )
void YM2413::setVol(int c, int volume)
{
	ch[c]->car->volume = volume;
}

void YM2413::Slot::setSlotVolume(int newVolume)
{
	volume = newVolume;
}

// Set F-Number (fnum : 9bit)
void YM2413::setFnumber(int c, int fnum)
{
	ch[c]->car->fnum = fnum;
	ch[c]->mod->fnum = fnum;
}

// Set Block data (block : 3bit)
void YM2413::setBlock(int c, int block)
{
	ch[c]->car->block = block;
	ch[c]->mod->block = block;
}

// Change Rhythm Mode
void YM2413::setRythmMode(int data)
{
	if (rythm_mode == (data&32)>>5 ) return;
	rythm_mode = (data&32)>>5;
	if (data&32) {
		// OFF->ON
		ch[6]->patch_number = 16;
		ch[7]->patch_number = 17;
		ch[8]->patch_number = 18;
		slot[SLOT_BD1]->setSlotPatch(patch[16*2+0]);
		slot[SLOT_BD2]->setSlotPatch(patch[16*2+1]);
		slot[SLOT_HH]-> setSlotPatch(patch[17*2+0]);
		slot[SLOT_SD]-> setSlotPatch(patch[17*2+1]);
		slot[SLOT_HH]->type = 1;
		slot[SLOT_TOM]->setSlotPatch(patch[18*2+0]);
		slot[SLOT_CYM]->setSlotPatch(patch[18*2+1]);
		slot[SLOT_TOM]->type = 1;
	} else {
		// ON->OFF
		setPatch(6, reg[0x36]>>4);
		setPatch(7, reg[0x37]>>4);
		slot[SLOT_HH]->type = 0;
		setPatch(8, reg[0x38]>>4);
		slot[SLOT_TOM]->type = 0;

		if(!(reg[0x26]&0x10)&&!(data&0x10))
			slot[SLOT_BD1]->eg_mode = FINISH;
		if(!(reg[0x26]&0x10)&&!(data&0x10))
			slot[SLOT_BD2]->eg_mode = FINISH;
		if(!(reg[0x27]&0x10)&&!(data&0x08))
			slot[SLOT_HH]->eg_mode = FINISH;
		if(!(reg[0x27]&0x10)&&!(data&0x04))
			slot[SLOT_SD]->eg_mode = FINISH;
		if(!(reg[0x28]&0x10)&&!(data&0x02))
			slot[SLOT_TOM]->eg_mode = FINISH;
		if(!(reg[0x28]&0x10)&&!(data&0x01))
			slot[SLOT_CYM]->eg_mode = FINISH;
	}
}

//***********************************************************//
//                                                           //
//                      Initializing                         //
//                                                           //
//***********************************************************//

// Constructor
YM2413::Slot::Slot(int type, short* volTab)
{
	this->type = type;
	this->volTab = volTab;
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
	eg_mode = SETTLE;
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
	patch = &null_patch;
}




// Constructor
YM2413::Channel::Channel(short* volTab)
{
	mod = new Slot(0, volTab);
	car = new Slot(1, volTab);
	patch_number = 0;
	key_status = false;
}

// Destructor
YM2413::Channel::~Channel()
{
	delete mod;
	delete car;
}

// reset channel
void YM2413::Channel::reset()
{
	mod->reset();
	car->reset();
	key_status = false;
}




// Constructor
YM2413::YM2413()
{
	for (int i=0; i<19*2; i++) {
		patch[i] = new Patch();
	}
	for (int i=0; i<9; i++) {
		ch[i] = new Channel(dB2LinTab);
		slot[i*2+0] = ch[i]->mod;
		slot[i*2+1] = ch[i]->car;
	}
	for (int i=0; i<18; i++) {
		slot[i]->plfo_am = &lfo_am;
		slot[i]->plfo_pm = &lfo_pm;
	}
	init();	// must be before registerSound()
	reset();
	reset_patch();

	setVolume(20000);	// TODO find a god value and put it in config file
	int bufSize = Mixer::instance()->registerSound(this);
	buffer = new int[bufSize];
}

// Reset patch datas by system default
void YM2413::reset_patch()
{
	for (int i = 0; i<19*2; i++)
		copyPatch(i, &default_patch[i]);
}

// Copy patch
void YM2413::copyPatch(int num, Patch *ptch)
{
	memcpy(patch[num], ptch, sizeof(Patch));
}

// Destructor
YM2413::~YM2413()
{
	Mixer::instance()->unregisterSound(this);
	for (int i=0; i<9; i++)
		delete ch[i];
	for (int i=0; i<19*2; i++)
		delete patch[i];
}

// Reset whole of OPLL except patch datas
void YM2413::reset()
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
	for(int i=0; i<9; i++) {
		ch[i]->reset();
		setPatch(i,0);
	}
	for (int i=0; i<0x40; i++)
		writeReg(i, 0);
	setInternalMute(true);	// set muted
}

void YM2413::setSampleRate(int sampleRate)
{
	if (!alreadyInitialized) {
		makeDphaseTable(sampleRate);
		makeDphaseARTable(sampleRate);
		makeDphaseDRTable(sampleRate);
		makeDphaseNoiseTable(sampleRate);
		pm_dphase = (int)rate_adjust(PM_SPEED*PM_DP_WIDTH/(CLOCK_FREQ/72), sampleRate);
		am_dphase = (int)rate_adjust(AM_SPEED*AM_DP_WIDTH/(CLOCK_FREQ/72), sampleRate);
		alreadyInitialized = true;
	}
}

void YM2413::init()
{
	if (!alreadyInitialized) {
		makePmTable();
		makeAmTable();
		makeAdjustTable();
		makeTllTable();
		makeRksTable();
		makeSinTable();
		makeDefaultPatch();
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
void YM2413::update_noise()
{
	if (noise_seed & 1)
		noise_seed ^= 0x24000;
	noise_seed >>= 1;
	whitenoise = noise_seed&1 ? DB_POS(6.0) : DB_NEG(6.0);

	noiseA_phase += noiseA_dphase;
	noiseB_phase += noiseB_dphase;

	noiseA_phase &= (0x40<<11) - 1;
	if ((noiseA_phase>>11)==0x3f)
		noiseA_phase = 0;
	noiseA = noiseA_phase&(0x03<<11)?DB_POS(6.0):DB_NEG(6.0);

	noiseB_phase &= (0x10<<11) - 1;
	noiseB = noiseB_phase&(0x0A<<11)?DB_POS(6.0):DB_NEG(6.0);
}

// Update AM, PM unit
void YM2413::update_ampm()
{
	pm_phase = (pm_phase + pm_dphase)&(PM_DP_WIDTH - 1);
	am_phase = (am_phase + am_dphase)&(AM_DP_WIDTH - 1);
	lfo_am = amtable[HIGHBITS(am_phase, AM_DP_BITS - AM_PG_BITS)];
	lfo_pm = pmtable[HIGHBITS(pm_phase, PM_DP_BITS - PM_PG_BITS)];
}

// PG
int YM2413::Slot::calc_phase()
{
	if (patch->PM) {
		phase += (dphase * (*(plfo_pm))) >> PM_AMP_BITS;
	} else {
		phase += dphase;
	}
	phase &= (DP_WIDTH - 1);
	return HIGHBITS(phase, DP_BASE_BITS);
}

// EG
int YM2413::Slot::calc_envelope()
{
	#define S2E(x) (SL2EG((int)(x/SL_STEP))<<(EG_DP_BITS-EG_BITS)) 
	int SL[16] = {
		S2E( 0), S2E( 3), S2E( 6), S2E( 9), S2E(12), S2E(15), S2E(18), S2E(21),
		S2E(24), S2E(27), S2E(30), S2E(33), S2E(36), S2E(39), S2E(42), S2E(48)
	};

	int egout;
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
		if (patch->EG == 0) {
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
		egout = (1<<EG_BITS) - 1;
		break ;
	default:
		egout = (1<<EG_BITS) - 1;
		break;
	}
	if (patch->AM) 
		egout = EG2DB(egout+tll) + *(plfo_am);
	else 
		egout = EG2DB(egout+tll);
	if (egout >= DB_MUTE)
		egout = DB_MUTE-1;
	return egout ;
}

// CARRIOR
int YM2413::Slot::calc_slot_car(int fm)
{
	egout = calc_envelope(); 
	pgout = calc_phase();
	output[1] = output[0];

	if (egout>=(DB_MUTE-1)) {
		output[0] = 0;
	} else {
		output[0] = volTab[sintbl[(pgout+wave2_8pi(fm))&(PG_WIDTH-1)] + egout];
	}
	return (output[1] + output[0]) >> 1;
}

// MODULATOR
int YM2413::Slot::calc_slot_mod()
{
	output[1] = output[0];
	egout = calc_envelope(); 
	pgout = calc_phase();

	if(egout>=(DB_MUTE-1)) {
		output[0] = 0;
	} else if (patch->FB!=0) {
		int fm = wave2_4pi(feedback) >> (7 - patch->FB);
		output[0] = volTab[sintbl[(pgout+fm)&(PG_WIDTH-1)] + egout];
	} else {
		output[0] = volTab[sintbl[pgout] + egout];
	}
	feedback = (output[1] + output[0]) >> 1;
	return feedback;
}

// TOM
int YM2413::Slot::calc_slot_tom()
{
	egout = calc_envelope(); 
	pgout = calc_phase();
	if (egout>=(DB_MUTE-1))
		return 0;
	return volTab[sintbl[pgout] + egout];
}

// SNARE
int YM2413::Slot::calc_slot_snare(int whitenoise)
{
	egout = calc_envelope();
	pgout = calc_phase();
	if (egout>=(DB_MUTE-1))
		return 0;
	if (pgout & (1<<(PG_BITS-1))) {
		return (volTab[egout] + volTab[egout+whitenoise]) >> 1;
	} else {
		return (volTab[DB_MUTE + DB_MUTE + egout] + volTab[egout+whitenoise]) >> 1;
	}
}

// TOP-CYM
int YM2413::Slot::calc_slot_cym(int a, int b)
{
	egout = calc_envelope();
	if (egout>=(DB_MUTE-1)) {
		return 0;
	} else {
		return (volTab[egout+a] + volTab[egout+b]) >> 1;
	}
}

// HI-HAT
int YM2413::Slot::calc_slot_hat(int a, int b, int whitenoise)
{
	egout = calc_envelope();
	if (egout>=(DB_MUTE-1)) {
		return 0;
	} else {
		return (volTab[egout+whitenoise] + volTab[egout+a] + volTab[egout+b]) >>2;
	}
}

int YM2413::calcSample()
{
	int inst = 0, perc = 0;

	// while muted updated_ampm() and update_noise() aren't called, probably ok
	update_ampm();
	update_noise();

	for(int i=0; i<6; i++) {
		if (ch[i]->car->eg_mode!=FINISH)
			inst += ch[i]->car->calc_slot_car(ch[i]->mod->calc_slot_mod());
	}
	if (!rythm_mode) {
		for(int i=6; i<9; i++) {
			if (ch[i]->car->eg_mode!=FINISH)
				inst += ch[i]->car->calc_slot_car(ch[i]->mod->calc_slot_mod());
		}
	} else {
		ch[7]->mod->pgout = ch[7]->mod->calc_phase();
		ch[8]->car->pgout = ch[8]->car->calc_phase();

		if (ch[6]->car->eg_mode!=FINISH)
			perc += ch[6]->car->calc_slot_car(ch[6]->mod->calc_slot_mod());
		if (ch[7]->mod->eg_mode!=FINISH)
			perc += ch[7]->mod->calc_slot_hat(noiseA, noiseB, whitenoise);
		if (ch[7]->car->eg_mode!=FINISH)
			perc += ch[7]->car->calc_slot_snare(whitenoise);
		if (ch[8]->mod->eg_mode!=FINISH)
			perc += ch[8]->mod->calc_slot_tom();
		if (ch[8]->car->eg_mode!=FINISH)
			perc += ch[8]->car->calc_slot_cym(noiseA, noiseB);
	}
	return inst + 2*perc; 
}

void YM2413::checkMute()
{
	setInternalMute(checkMuteHelper());
}
bool YM2413::checkMuteHelper()
{
	//TODO maybe also check volume -> more often mute ??
	for (int i=0; i<6; i++) {
		if (ch[i]->car->eg_mode!=FINISH) return false;
	}
	if (!rythm_mode) {
		for(int i=6; i<9; i++) {
			 if (ch[i]->car->eg_mode!=FINISH) return false;
		}
	} else {
		if (ch[6]->car->eg_mode!=FINISH) return false;
		if (ch[7]->mod->eg_mode!=FINISH) return false;
		if (ch[7]->car->eg_mode!=FINISH) return false;
		if (ch[8]->mod->eg_mode!=FINISH) return false;
		if (ch[8]->car->eg_mode!=FINISH) return false;
	}
	return true;	// nothing is playing, then mute
}

int* YM2413::updateBuffer(int length)
{
	PRT_DEBUG("update YM2413 buffer");
	int* buf = buffer;
	while (length) {
		*(buf++) = calcSample();
		length--;
	}
	checkMute();
	return buffer;
}


//**************************************************//
//                                                  //
//                       I/O Ctrl                   //
//                                                  //
//**************************************************//

void YM2413::writeReg(byte regis, byte data)
{
	int j, v, cha;
	
	assert (regis < 0x40);
	switch (regis) {
	case 0x00:
		patch[0]->AM = (data>>7)&1;
		patch[0]->PM = (data>>6)&1;
		patch[0]->EG = (data>>5)&1;
		patch[0]->KR = (data>>4)&1;
		patch[0]->ML = (data)&15;
		for (int i=0; i<9; i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->mod->updatePG();
				ch[i]->mod->updateRKS();
				ch[i]->mod->updateEG();
			}
		}
		break;
	case 0x01:
		patch[1]->AM = (data>>7)&1;
		patch[1]->PM = (data>>6)&1;
		patch[1]->EG = (data>>5)&1;
		patch[1]->KR = (data>>4)&1;
		patch[1]->ML = (data)&15;
		for (int i=0;i<9;i++) {
			if(ch[i]->patch_number==0) {
				ch[i]->car->updatePG();
				ch[i]->car->updateRKS();
				ch[i]->car->updateEG();
			}
		}
		break;
	case 0x02:
		patch[0]->KL = (data>>6)&3;
		patch[0]->TL = (data)&63;
		for (int i=0;i<9;i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->mod->updateTLL() ;
			}
		}
		break;
	case 0x03:
		patch[1]->KL = (data>>6)&3;
		patch[1]->WF = (data>>4)&1;
		patch[0]->WF = (data>>3)&1;
		patch[0]->FB = (data)&7;
		for (int i=0;i<9;i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->mod->updateWF();
				ch[i]->car->updateWF();
			}
		}
		break;
	case 0x04:
		patch[0]->AR = (data>>4)&15;
		patch[0]->DR = (data)&15;
		for (int i=0;i<9;i++) {
			if(ch[i]->patch_number==0) {
				ch[i]->mod->updateEG() ;
			}
		}
		break;
	case 0x05:
		patch[1]->AR = (data>>4)&15;
		patch[1]->DR = (data)&15;
		for (int i=0;i<9;i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->car->updateEG();
			}
		}
		break;
	case 0x06:
		patch[0]->SL = (data>>4)&15;
		patch[0]->RR = (data)&15;
		for (int i=0;i<9;i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->mod->updateEG();
			}
		}
		break;
	case 0x07:
		patch[1]->SL = (data>>4)&15;
		patch[1]->RR = (data)&15;
		for (int i=0;i<9;i++) {
			if (ch[i]->patch_number==0) {
				ch[i]->car->updateEG();
			}
		}
		break;
	case 0x0e:
		if (rythm_mode) {
			slot_on_flag[SLOT_BD1] = (reg[0x0e]&0x10) | (reg[0x26]&0x10);
			slot_on_flag[SLOT_BD2] = (reg[0x0e]&0x10) | (reg[0x26]&0x10);
			slot_on_flag[SLOT_SD]  = (reg[0x0e]&0x08) | (reg[0x27]&0x10);
			slot_on_flag[SLOT_HH]  = (reg[0x0e]&0x01) | (reg[0x27]&0x10);
			slot_on_flag[SLOT_TOM] = (reg[0x0e]&0x04) | (reg[0x28]&0x10);
			slot_on_flag[SLOT_CYM] = (reg[0x0e]&0x02) | (reg[0x28]&0x10);
		} else {
			slot_on_flag[SLOT_BD1] = (reg[0x26]&0x10);
			slot_on_flag[SLOT_BD2] = (reg[0x26]&0x10);
			slot_on_flag[SLOT_SD]  = (reg[0x27]&0x10);
			slot_on_flag[SLOT_HH]  = (reg[0x27]&0x10);
			slot_on_flag[SLOT_TOM] = (reg[0x28]&0x10);
			slot_on_flag[SLOT_CYM] = (reg[0x28]&0x10);
		}
		if (((data>>5)&1)!=(rythm_mode)) {
			setRythmMode(data);
		}
		if (rythm_mode) {
			if (data&0x10) keyOn_BD(); else keyOff_BD();
			if (data&0x8) keyOn_SD(); else keyOff_SD();
			if (data&0x4) keyOn_TOM(); else keyOff_TOM();
			if (data&0x2) keyOn_CYM(); else keyOff_CYM();
			if (data&0x1) keyOn_HH(); else keyOff_HH();
		}
		ch[6]->mod->updateAll();
		ch[6]->car->updateAll();
		ch[7]->mod->updateAll();  
		ch[7]->car->updateAll();
		ch[8]->mod->updateAll();
		ch[8]->car->updateAll();        
		break;
	case 0x0f:
		break ;
	case 0x10:  case 0x11:  case 0x12:  case 0x13:
	case 0x14:  case 0x15:  case 0x16:  case 0x17:
	case 0x18:
		cha = regis-0x10;
		setFnumber(cha, data + ((reg[0x20+cha]&1)<<8));
		ch[cha]->mod->updateAll();
		ch[cha]->car->updateAll();
		switch(regis) {
		case 0x17:
			noiseA_dphase = dphaseNoiseTable[data + ((reg[0x27]&1)<<8)][(reg[0x27]>>1)&7];
			break;
		case 0x18:
			noiseB_dphase = dphaseNoiseTable[data + ((reg[0x28]&1)<<8)][(reg[0x28]>>1)&7];
			break;
		default:
			break;
		}
		break;
	case 0x20:  case 0x21:  case 0x22:  case 0x23:
	case 0x24:  case 0x25:  case 0x26:  case 0x27:
	case 0x28:
		cha = regis - 0x20;
		setFnumber(cha, ((data&1)<<8) + reg[0x10+cha]);
		setBlock(cha, (data>>1)&7 );
		slot_on_flag[cha*2] = slot_on_flag[cha*2+1] = (reg[regis])&0x10;
		switch(regis) {
		case 0x26:
			if (rythm_mode) {
				slot_on_flag[SLOT_BD1] |= (reg[0x0e])&0x10;
				slot_on_flag[SLOT_BD2] |= (reg[0x0e])&0x10;
			}
			break;
		case 0x27:
			noiseA_dphase = dphaseNoiseTable[((data&1)<<8) + reg[0x17]][(data>>1)&7];
			if (rythm_mode) {
				slot_on_flag[SLOT_SD]  |= (reg[0x0e])&0x08;
				slot_on_flag[SLOT_HH]  |= (reg[0x0e])&0x01;
			}
			break;
		case 0x28:
			noiseB_dphase = dphaseNoiseTable[((data&1)<<8) + reg[0x18]][(data>>1)&7];
			if (rythm_mode) {
				slot_on_flag[SLOT_TOM] |= (reg[0x0e])&0x04;
				slot_on_flag[SLOT_CYM] |= (reg[0x0e])&0x02;
			}
			break;
		default:
			break;
		}
		if ((reg[regis]^data)&0x20) setSustine(cha, (data>>5)&1);
		if (data&0x10) keyOn(cha); else keyOff(cha);
		ch[cha]->mod->updateAll();
		ch[cha]->car->updateAll();
		break;
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37: case 0x38:
		j = (data>>4)&15;
		v = data&15;
		if ((rythm_mode)&&(regis>=0x36)) {
			switch(regis) {
			case 0x37:
				ch[7]->mod->setSlotVolume(j<<2);
				break;
			case 0x38:
				ch[8]->mod->setSlotVolume(j<<2);
				break;
			}
		} else { 
			setPatch(regis-0x30, j);
		}
		setVol(regis-0x30, v<<2);
		ch[regis-0x30]->mod->updateAll();
		ch[regis-0x30]->car->updateAll();
		break;
	default:
		break;
	}
	reg[regis] = (byte)data;
	checkMute();
}
