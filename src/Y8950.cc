// $Id$

//****************************************************************************//
//                                                                            //
//  emu8950.c -- Y8950 emulator : written by Mitsutaka Okazaki 2001           //
//                                                                            //
//  2001 05-19 : Version 0.10 -- Test release (No rythm, No pcm).             //
//                                                                            //
//  References:                                                               //
//    fmopl.c        -- 1999,2000 written by Tatsuyuki Satoh.                 //
//    s_opl.c        -- 2001 written by Mamiya.                               //
//    fmgen.cpp      -- 1999,2000 written by cisc.                            //
//    MSX-Datapack                                                            //
//    YMU757 data sheet                                                       //
//    YM2143 data sheet                                                       //
//                                                                            //
//****************************************************************************//
#include <math.h>
#include "Y8950.hh"
#include "Mixer.hh"

int Y8950::dB2LinTab[(2*DB_MUTE)*2];
int Y8950::Slot::fullsintable[PG_WIDTH];
int Y8950::Slot::dphaseARTable[16][16];
int Y8950::Slot::dphaseDRTable[16][16];
int Y8950::Slot::tllTable[16][8][1<<TL_BITS][4];
int Y8950::Slot::rksTable[2][8][2];
int Y8950::Slot::dphaseTable[1024][8][16];
int Y8950::Slot::AR_ADJUST_TABLE[1<<EG_BITS];


//**************************************************//
//                                                  //
//                  Create tables                   //
//                                                  //
//**************************************************//

int Y8950::CLAP(int min, int x, int max)
{
	return (x<min) ? min : ((max<x) ? max : x);
}

int Y8950::Slot::ALIGN(int d, double SS, double SD) 
{ 
	return d*(int)(SS/SD);
}

int Y8950::HIGHBITS(int c, int b)
{
	return c >> b;
}

int Y8950::LOWBITS(int c, int b)
{
	return c & ((1<<b)-1);
}

int Y8950::EXPAND_BITS(int x, int s, int d)
{
	return x << (d-s);
}

int Y8950::EXPAND_BITS_X(int x, int s, int d)
{
	return (x << (d-s)) | ((1 << (d-s)) - 1);
}

int Y8950::rate_adjust(int x, int rate)
{
	return (int)((double)x*CLOCK_FREQ/72/rate + 0.5); // +0.5 to round
}

// Table for AR to LogCurve. 
void Y8950::Slot::makeAdjustTable()
{
	AR_ADJUST_TABLE[0] = 1<<EG_BITS;
	for (int i=1; i < (1<<EG_BITS); i++)
		AR_ADJUST_TABLE[i] = (int)((double)(1<<EG_BITS)-1-(1<<EG_BITS)*log(i)/log(1<<EG_BITS)) >> 1; 
}

void Y8950::setInternalVolume(short maxVolume)
{
	Slot::makeDB2LinTable(maxVolume);
}
// Table for dB(0 -- (1<<DB_BITS)) to Liner(0 -- DB2LIN_AMP_WIDTH) 
void Y8950::Slot::makeDB2LinTable(short maxVolume)
{
	for (int i=0; i < 2*DB_MUTE; i++) {
		dB2LinTab[i] = (i<DB_MUTE) ?
			(int)((double)maxVolume*pow(10,-(double)i*DB_STEP/20)) :
			0;
		dB2LinTab[i+ DB_MUTE + DB_MUTE] = -dB2LinTab[i];
	}
}

// Liner(+0.0 - +1.0) to dB((1<<DB_BITS) - 1 -- 0) 
int Y8950::Slot::lin2db(double d)
{
	if (d == 0) 
		return (DB_MUTE-1);
	else { 
		int tmp = -(int)(20.0*log10(d)/DB_STEP);
		if (tmp<DB_MUTE-1)
			return tmp;
		else
			return DB_MUTE-1;
	}
}

// Sin Table 
void Y8950::Slot::makeSinTable()
{
	for (int i=0; i < PG_WIDTH/4; i++)
		fullsintable[i] = lin2db(sin(2.0*PI*i/PG_WIDTH));
	for (int i=0; i < PG_WIDTH/4; i++)
		fullsintable[PG_WIDTH/2 - 1 - i] = fullsintable[i];
	for (int i=0; i < PG_WIDTH/2; i++)
		fullsintable[PG_WIDTH/2+i] = DB_MUTE + DB_MUTE + fullsintable[i];
}

// Table for Pitch Modulator 
void Y8950::makePmTable()
{
	for (int i=0; i<PM_PG_WIDTH; i++)
		pmtable[0][i] = (int)((double)PM_AMP * pow(2,(double)PM_DEPTH*sin(2.0*PI*i/PM_PG_WIDTH)/1200));
	for (int i=0; i < PM_PG_WIDTH; i++)
		pmtable[1][i] = (int)((double)PM_AMP * pow(2,(double)PM_DEPTH2*sin(2.0*PI*i/PM_PG_WIDTH)/1200));
}

// Table for Amp Modulator 
void Y8950::makeAmTable()
{
	for (int i=0; i<AM_PG_WIDTH; i++)
		amtable[0][i] = (int)((double)AM_DEPTH/2/DB_STEP * (1.0 + sin(2.0*PI*i/PM_PG_WIDTH)));
	for (int i=0; i<AM_PG_WIDTH; i++)
		amtable[1][i] = (int)((double)AM_DEPTH2/2/DB_STEP * (1.0 + sin(2.0*PI*i/PM_PG_WIDTH)));
}

// Phase increment counter table  
void Y8950::Slot::makeDphaseTable(int sampleRate)
{
	int mltable[16] = {
		1,1*2,2*2,3*2,4*2,5*2,6*2,7*2,8*2,9*2,10*2,10*2,12*2,12*2,15*2,15*2
	};

	for (int fnum=0; fnum<1024; fnum++)
		for (int block=0; block<8; block++)
			for (int ML=0; ML<16; ML++)
				dphaseTable[fnum][block][ML] = 
					rate_adjust((((fnum * mltable[ML]) << block ) >> (21 - DP_BITS)), sampleRate);
}

void Y8950::Slot::makeTllTable()
{
	#define dB2(x) (int)((x)*2)
	static int kltable[16] = {
		dB2( 0.000),dB2( 9.000),dB2(12.000),dB2(13.875),
		dB2(15.000),dB2(16.125),dB2(16.875),dB2(17.625),
		dB2(18.000),dB2(18.750),dB2(19.125),dB2(19.500),
		dB2(19.875),dB2(20.250),dB2(20.625),dB2(21.000)
	};

	for(int fnum=0; fnum<16; fnum++)
		for(int block=0; block<8; block++)
			for(int TL=0; TL<64; TL++)
				for(int KL=0; KL<4; KL++) {
					if (KL==0) {
						tllTable[fnum][block][TL][KL] = ALIGN(TL, TL_STEP, EG_STEP);
					} else {
						int tmp = kltable[fnum] - dB2(3.000) * (7 - block);
						if (tmp <= 0)
							tllTable[fnum][block][TL][KL] = ALIGN(TL, TL_STEP, EG_STEP);
						else 
							tllTable[fnum][block][TL][KL] = (int)((tmp>>(3-KL))/EG_STEP) + ALIGN(TL, TL_STEP, EG_STEP);
					}
				}
}


// Rate Table for Attack 
void Y8950::Slot::makeDphaseARTable(int sampleRate)
{
	for(int AR=0; AR<16; AR++)
		for(int Rks=0; Rks<16; Rks++) {
			int RM = AR + (Rks>>2);
			int RL = Rks&3;
			if (RM>15) RM=15;
			switch (AR) { 
			case 0:
				dphaseARTable[AR][Rks] = 0;
				break;
			case 15:
				dphaseARTable[AR][Rks] = EG_DP_WIDTH;
				break;
			default:
				dphaseARTable[AR][Rks] = rate_adjust((3*(RL+4) << (RM+1)), sampleRate);
				break;
			}
		}
}

// Rate Table for Decay 
void Y8950::Slot::makeDphaseDRTable(int sampleRate)
{
	for(int DR=0; DR<16; DR++)
		for(int Rks=0; Rks<16; Rks++) {
			int RM = DR + (Rks>>2);
			int RL = Rks&3;
			if (RM>15) RM=15;
			switch (DR) { 
			case 0:
				dphaseDRTable[DR][Rks] = 0;
				break;
			default:
				dphaseDRTable[DR][Rks] = rate_adjust((RL+4) << (RM-1), sampleRate);
				break;
			}
		}
}

void Y8950::Slot::makeRksTable()
{
	for(int fnum9=0; fnum9<2; fnum9++)
		for(int block=0; block<8; block++)
			for(int KR=0; KR<2; KR++) {
				if (KR!=0)
					rksTable[fnum9][block][KR] = (block<<1) + fnum9;
				else
					rksTable[fnum9][block][KR] = block >> 1;
			}
}

//***********************************************************//
//                                                           //
// Calc Parameters                                           //
//                                                           //
//***********************************************************//

int Y8950::Slot::calc_eg_dphase()
{
	switch (eg_mode) {
	case ATTACK:
		return dphaseARTable[patch.AR][rks];
	case DECAY:
		return dphaseDRTable[patch.DR][rks];
	case SUSHOLD:
		return 0;
	case SUSTINE:
		return dphaseDRTable[patch.RR][rks];
	case RELEASE:
		if(patch.EG)
			return dphaseDRTable[patch.RR][rks];
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
//  opl internal interfaces                                   //
//                                                            //
//************************************************************//

void Y8950::Slot::UPDATE_PG()
{
	dphase = dphaseTable[fnum][block][patch.ML];
}

void Y8950::Slot::UPDATE_TLL()
{
	tll = tllTable[fnum>>6][block][patch.TL][patch.KL];
}

void Y8950::Slot::UPDATE_RKS()
{
	rks = rksTable[fnum>>9][block][patch.KR];
}

void Y8950::Slot::UPDATE_EG()
{
	eg_dphase = calc_eg_dphase();
}

void Y8950::Slot::UPDATE_ALL()
{
	UPDATE_PG();
	UPDATE_TLL();
	UPDATE_RKS();
	UPDATE_EG(); // EG should be last 
}

// Slot key on  
void Y8950::Slot::slotOn()
{
	eg_mode = ATTACK;
	phase = 0;
	eg_phase = 0;
}

// Slot key off 
void Y8950::Slot::slotOff()
{
	if (eg_mode == ATTACK)
		eg_phase = EXPAND_BITS(AR_ADJUST_TABLE[HIGHBITS(eg_phase, EG_DP_BITS-EG_BITS)], EG_BITS, EG_DP_BITS);
	eg_mode = RELEASE;
}

// Channel key on 
void Y8950::keyOn(int i)
{
	ch[i].mod.slotOn();
	ch[i].car.slotOn();
}

// Channel key off 
void Y8950::keyOff(int i)
{
	ch[i].mod.slotOff();
	ch[i].car.slotOff();
}

void Y8950::keyOn_BD  () { keyOn(6); }
void Y8950::keyOn_SD  () { if (!slot_on_flag[SLOT_SD])  ch[7].car.slotOn(); }
void Y8950::keyOn_TOM () { if (!slot_on_flag[SLOT_TOM]) ch[8].mod.slotOn(); }
void Y8950::keyOn_HH  () { if (!slot_on_flag[SLOT_HH])  ch[7].mod.slotOn(); }
void Y8950::keyOn_CYM () { if (!slot_on_flag[SLOT_CYM]) ch[8].car.slotOn(); }
void Y8950::keyOff_BD () { keyOff(6); }
void Y8950::keyOff_SD () { if (slot_on_flag[SLOT_SD])   ch[7].car.slotOff(); }
void Y8950::keyOff_TOM() { if (slot_on_flag[SLOT_TOM])  ch[8].mod.slotOff(); }
void Y8950::keyOff_HH () { if (slot_on_flag[SLOT_HH])   ch[7].mod.slotOff(); }
void Y8950::keyOff_CYM() { if (slot_on_flag[SLOT_CYM])  ch[8].car.slotOff(); }

// Set F-Number ( fnum : 10bit ) 
void Y8950::setFnumber(int c, int fnum)
{
	ch[c].car.fnum = fnum;
	ch[c].mod.fnum = fnum;
}

// Set Block data (block : 3bit ) 
void Y8950::setBlock(int c, int block)
{
	ch[c].car.block = block;
	ch[c].mod.block = block;
}

//**********************************************************//
//                                                          //
//  Initializing                                            //
//                                                          //
//**********************************************************//

Y8950::Patch::Patch()
{
	reset();
}

void Y8950::Patch::reset()
{
	TL = FB = EG = ML = AR = DR = SL = RR = KR = KL = AM = PM = 0;
}


Y8950::Slot::Slot()
{
}

Y8950::Slot::~Slot()
{
}

void Y8950::Slot::reset()
{
	sintbl = fullsintable;
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
	fnum = 0;
	block = 0;
	pgout = 0;
	egout = 0;
	patch.reset();
	UPDATE_ALL();
}


Y8950::Channel::Channel()
{
	reset();
}

Y8950::Channel::~Channel()
{
}

void Y8950::Channel::reset()
{
	mod.reset();
	car.reset();
	alg = false;
}


Y8950::Y8950(short volume, const EmuTime &time)
{
	makePmTable();
	makeAmTable();
	Y8950::Slot::makeAdjustTable();
	Y8950::Slot::makeTllTable();
	Y8950::Slot::makeRksTable();
	Y8950::Slot::makeSinTable();

	for (int i=0; i<9; i++) {
		slot[i*2+0] = &(ch[i].mod);
		slot[i*2+1] = &(ch[i].car);
	}
	for (int i=0; i<10; i++) 
		mask[i] = 0;
	
	// adpcm
	// 256Kbytes RAM 
	memory[0] = new byte[256*1024];
	// 256Kbytes ROM 
	memory[1] = new byte[256*1024];
	
	reset(time);

	setVolume(volume);
	int bufSize = Mixer::instance()->registerSound(this);
	buffer = new int[bufSize];
}

Y8950::~Y8950()
{
	Mixer::instance()->unregisterSound(this);
	delete[] buffer;
	delete[] memory[0];
	delete[] memory[1];
}

void Y8950::setSampleRate(int sampleRate)
{
	this->sampleRate = sampleRate;
	Y8950::Slot::makeDphaseTable(sampleRate);
	Y8950::Slot::makeDphaseARTable(sampleRate);
	Y8950::Slot::makeDphaseDRTable(sampleRate);
	pm_dphase = (int)rate_adjust(PM_SPEED * PM_DP_WIDTH / (CLOCK_FREQ/72), sampleRate );
	am_dphase = (int)rate_adjust(AM_SPEED * AM_DP_WIDTH / (CLOCK_FREQ/72), sampleRate );
}

// Reset whole of opl except patch datas. 
void Y8950::reset(const EmuTime &time)
{
	for (int i=0; i<9; i++) 
		ch[i].reset();
	output[0] = 0;
	output[1] = 0;

	rythm_mode = false;
	am_mode = 0;
	pm_mode = 0;
	pm_phase = 0;
	am_phase = 0;
	noise_seed =0xffff;

	// update the output buffer before changing the register
	Mixer::instance()->updateStream(time);
	for (int i=0; i<0xFF; i++) 
		reg[i] = 0x00;
	for (int i=0; i<18; i++) 
		slot_on_flag[i] = 0;

	// adpcm
	reg[0x04] = 0x18;
	play_start = false;
	status = 0x06;	// TODO
	play_addr = 0;
	start_addr = 0;
	stop_addr = 0;
	delta_addr = 0;
	delta_n = 0;
	wave = memory[0];
	play_addr_mask = reg[0x08]&R08_64K ? (1<<17)-1 : (1<<19)-1;
	
	setInternalMute(true);	// muted
}


//********************************************************//
//                                                        //
// Generate wave data                                     //
//                                                        //
//********************************************************//

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 4PI). 
int Y8950::Slot::wave2_4pi(int e)
{
	int shift =  SLOT_AMP_BITS - PG_BITS - 1;
	if (shift > 0)
		return e >> shift;
	else
		return e << -shift;
}

// Convert Amp(0 to EG_HEIGHT) to Phase(0 to 8PI). 
int Y8950::Slot::wave2_8pi(int e)
{
	int shift = SLOT_AMP_BITS - PG_BITS - 2;
	if (shift > 0)
		return e >> shift;
	else
		return e << -shift;
}


void Y8950::update_noise()
{
	noise_seed = ((noise_seed>>15)^((noise_seed>>12)&1)) | ((noise_seed<<1)&0xffff);
	whitenoise = noise_seed & 1; 
}

void Y8950::update_ampm()
{
	pm_phase = (pm_phase + pm_dphase)&(PM_DP_WIDTH - 1);
	am_phase = (am_phase + am_dphase)&(AM_DP_WIDTH - 1);
	lfo_am = amtable[am_mode][HIGHBITS(am_phase, AM_DP_BITS - AM_PG_BITS)];
	lfo_pm = pmtable[pm_mode][HIGHBITS(pm_phase, PM_DP_BITS - PM_PG_BITS)];
}

int Y8950::Slot::calc_phase(int lfo_pm)
{
	if (patch.PM)
		phase += (dphase * lfo_pm) >> PM_AMP_BITS;
	else
		phase += dphase;
	phase &= (DP_WIDTH - 1);
	return HIGHBITS(phase, DP_BASE_BITS);
}

int Y8950::Slot::calc_envelope(int lfo_am)
{
	#define S2E(x) (ALIGN((int)(x/SL_STEP),SL_STEP,EG_STEP)<<(EG_DP_BITS-EG_BITS)) 
	static int SL[16] = {
		S2E( 0), S2E( 3), S2E( 6), S2E( 9), S2E(12), S2E(15), S2E(18), S2E(21),
		S2E(24), S2E(27), S2E(30), S2E(33), S2E(36), S2E(39), S2E(42), S2E(93)
	};
	
	int egout;
	switch (eg_mode) {
	case ATTACK:
		eg_phase += eg_dphase;
		if (EG_DP_WIDTH & eg_phase) {
			egout = 0;
			eg_phase= 0;
			eg_mode = DECAY;
			UPDATE_EG();
		} else {
			egout = AR_ADJUST_TABLE[HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS)];
		}
		break;

	case DECAY:
		eg_phase += eg_dphase;
		egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		if (eg_phase >= SL[patch.SL]) {
			if(patch.EG) {
				eg_phase = SL[patch.SL];
				eg_mode = SUSHOLD;
				UPDATE_EG();
			} else {
				eg_phase = SL[patch.SL];
				eg_mode = SUSTINE;
				UPDATE_EG();
			}
			egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		}
		break;

	case SUSHOLD:
		egout = HIGHBITS(eg_phase, EG_DP_BITS - EG_BITS);
		if (patch.EG == 0) {
			eg_mode = SUSTINE;
			UPDATE_EG();
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
		break;

	default:
		egout = (1<<EG_BITS) - 1;
		break;
	}

	if (patch.AM)
		egout = ALIGN(egout+tll,EG_STEP,DB_STEP) + lfo_am;
	else 
		egout = ALIGN(egout+tll,EG_STEP,DB_STEP);
	if (egout>=DB_MUTE)
		egout = DB_MUTE-1;
	return egout;
}

int Y8950::Slot::calc_slot_car(int lfo_pm, int lfo_am, int fm)
{
	egout = calc_envelope(lfo_am); 
	pgout = calc_phase(lfo_pm);
	if (egout>=(DB_MUTE-1))
		return 0;
	return dB2LinTab[sintbl[(pgout+wave2_8pi(fm))&(PG_WIDTH-1)] + egout];
}

int Y8950::Slot::calc_slot_mod(int lfo_pm, int lfo_am)
{
	output[1] = output[0];
	egout = calc_envelope(lfo_am); 
	pgout = calc_phase(lfo_pm);

	if (egout>=(DB_MUTE-1)) {
		output[0] = 0;
	} else if (patch.FB!=0) {
		int fm = wave2_4pi(feedback) >> (7-patch.FB);
		output[0] = dB2LinTab[sintbl[(pgout+fm)&(PG_WIDTH-1)] + egout];
	} else
		output[0] = dB2LinTab[sintbl[pgout] + egout];
	
	feedback = (output[1] + output[0])>>1;
	return feedback;
}

int Y8950::calcSample()
{
	// while muted update_ampm() and update_noise() aren't called, probably ok
	update_ampm();
	update_noise();      

	int inst = 0;
	for (int i=0; i<6; i++)
	{
		if ((!mask[i])&&(ch[i].car.eg_mode!=FINISH)) {
			if (ch[i].alg)
				inst += ch[i].car.calc_slot_car(lfo_pm, lfo_am, 0) + ch[i].mod.calc_slot_mod(lfo_pm, lfo_am);
			else
				inst += ch[i].car.calc_slot_car(lfo_pm, lfo_am, ch[i].mod.calc_slot_mod(lfo_pm, lfo_am));
		}
	}
	if (!rythm_mode) {
		for (int i=6; i<9; i++) {
			if ((!mask[i])&&(ch[i].car.eg_mode!=FINISH)) {
				if (ch[i].alg)
					inst += ch[i].car.calc_slot_car(lfo_pm, lfo_am, 0) + ch[i].mod.calc_slot_mod(lfo_pm, lfo_am);
				else
					inst += ch[i].car.calc_slot_car(lfo_pm, lfo_am, ch[i].mod.calc_slot_mod(lfo_pm, lfo_am));
			}
		}
	}
	return  inst + calcAdpcm();
}


// Update ADPCM data stage (Register 0x0F) 
bool Y8950::update_stage()
{
	delta_addr += delta_n;
	if (delta_addr & DELTA_ADDR_MAX) {
		delta_addr &= DELTA_ADDR_MASK;
		play_addr = (play_addr+1) & play_addr_mask;
		if (play_addr == (stop_addr & play_addr_mask)) {
			if (reg[0x07] & R07_REPEAT) {
				play_addr = start_addr & play_addr_mask;
			} else {
				play_start = false;
				status |= STATUS_EOS;
			}
		} else {
			reg[0x0F] = wave[play_addr>>1];
		}
		return true;
	}
	return false;
}

void Y8950::update_output(nibble val)
{
	// This table values are from ymdelta.c by Tatsuyuki Satoh.
	static int F[] = {
		57,  57,  57,  57,  77, 102, 128, 153
	};

	adpcmOutput[1] = adpcmOutput[0];
	if (val&8)
		adpcmOutput[0] -= (diff * ((val&7)*2+1)) >> 3;
	else
		adpcmOutput[0] += (diff * ((val&7)*2+1)) >> 3;
	adpcmOutput[0] = CLAP(DECODE_MIN, adpcmOutput[0], DECODE_MAX);
	diff = CLAP(DMIN, (diff * F[val&7])>>6, DMAX);
}

int Y8950::calcAdpcm()
{
	if (reg[0x07] & R07_SP_OFF) 
		return 0;
	if (play_start && update_stage()) {
		nibble val;
		if (play_addr&1)
			val = reg[0x0f]&0x0f;
		else
			val = reg[0x0f]>>4;
		update_output(val);
	}
	return ((adpcmOutput[0] + adpcmOutput[1]) * reg[0x12]) >> 13;
}


void Y8950::checkMute()
{
	bool mute = checkMuteHelper();
	PRT_DEBUG("Y8950: muted " << mute);
	setInternalMute(mute);
}
bool Y8950::checkMuteHelper()
{
	//TODO maybe also check volume -> more often mute ??
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

	//TODO adpcm
	
	return true;	// nothing is playing, then mute
}

int* Y8950::updateBuffer(int length)
{
	PRT_DEBUG("Y8950: update buffer");
	if (isInternalMuted()) {
		PRT_DEBUG("Y8950: muted");
		return NULL;
	}

	//TODO
	//int channelMask = 0;
	//for (int i = 9; i--; ) {
	//	channelMask <<= 1;
	//	if (ch[i].car.eg_mode != FINISH) channelMask |= 1;
	//}

	int* buf = buffer;
	while (length--) {
		*(buf++) = calcSample();
	}

	checkMute();
	return buffer;
}

//**************************************************//
//                                                  //
//                       I/O Ctrl                   //
//                                                  //
//**************************************************//

void Y8950::writeReg(byte rg, byte data, const EmuTime &time)
{
	PRT_DEBUG("Y8950 write " << (int)rg << " " << (int)data);
	int stbl[32] = {
		 0, 2, 4, 1, 3, 5,-1,-1,
		 6, 8,10, 7, 9,11,-1,-1,
		12,14,16,13,15,17,-1,-1,
		-1,-1,-1,-1,-1,-1,-1,-1
	};

	//TODO only for registers that influence sound
	// update the output buffer before changing the register
	Mixer::instance()->updateStream(time);

	if (rg<0x20) {
		switch (rg) {
		case 0x01: // TEST
			// TODO
			// Y8950 MSX-AUDIO Test register $01 (write only)
			//
			// Bit	Description
			//
			// 7	Reset LFOs - seems to force the LFOs to their initial values (eg.
			//	maximum amplitude, zero phase deviation)
			//
			// 6	something to do with ADPCM - bit 0 of the status register is
			//	affected by setting this bit (PCM BSY)
			//
			// 5	No effect? - Waveform select enable in YM3812 OPL2 so seems
			//	reasonable that this bit wouldn't have been used in OPL
			//
			// 4	No effect?
			//
			// 3	Faster LFOs - increases the frequencies of the LFOs and (maybe)
			//	the timers (cf. YM2151 test register)
			//
			// 2	Reset phase generators - No phase generator output, but envelope
			//	generators still work (can hear a transient when they are gated)
			//
			// 1	No effect?
			//
			// 0	Reset envelopes - Envelope generator outputs forced to maximum,
			//	so all enabled voices sound at maximum
			reg[0x01] = data;
			break;

		case 0x02: // TIMER1 (reso. 80us)
			// TODO
			reg[0x02] = data;
			break;

		case 0x03: // TIMER2 (reso. 320us) 
			// TODO
			reg[0x03] = data;
			break;

		case 0x04: // FLAG CONTROL 
			if (data & R04_IRQ_RESET) {
				status &= 0x07;	//TODO
			} else {
				reg[0x04] = data;
			}
			break;

		case 0x05: // (KEYBOARD IN)
			// TODO
			// read-only
			break;

		case 0x06: // (KEYBOARD OUT) 
			// TODO
			reg[0x06] = data;
			break;

		case 0x07: // START/REC/MEM DATA/REPEAT/SP-OFF/-/-/RESET 
			if (data & R07_RESET) {
				play_start = false;
				break;
			} else if (data & R07_START) {
				play_start = true;
				play_addr = start_addr & play_addr_mask;
				delta_addr = 0;
				adpcmOutput[0] = 0;
				adpcmOutput[1] = 0;
				diff = DDEF;
			}
			reg[0x07] = data;
			break;

		case 0x08: // CSM/KEY BOARD SPLIT/-/-/SAMPLE/DA AD/64K/ROM 
			reg[0x08] = data;
			wave = reg[0x08]&R08_ROM ? memory[1] : memory[0];
			play_addr_mask = reg[0x08]&R08_64K ? (1<<17)-1 : (1<<19)-1;
			break;

		case 0x09: // START ADDRESS (L) 
		case 0x0A: // START ADDRESS (H) 
			reg[rg] = data;
			start_addr = ((reg[0x0A]<<8)|reg[0x09]) << 3;
			break;

		case 0x0B: // STOP ADDRESS (L) 
		case 0x0C: // STOP ADDRESS (H) 
			reg[rg] = data;
			stop_addr = (((reg[0x0C])<<8)|reg[0x0B]) << 3;
			break;

		case 0x0D: // PRESCALE (L) 
		case 0x0E: // PRESCALE (H) 
			reg[rg] = data;
			break;

		case 0x0F: // ADPCM-DATA 
			reg[0x0F] = data;
			if ((reg[0x07]&R07_REC) && (reg[0x07]&R07_MEMORY_DATA)) {
				wave[play_addr>>1] = data;
				play_addr = (play_addr+2)&(play_addr_mask);
				if (play_addr >= (stop_addr & play_addr_mask)) {
					//status |= STATUS_EOS; // Bug? 
				}
			}
			status |= STATUS_BUF_RDY;
			break;

		case 0x10: // DELTA-N (L) 
		case 0x11: // DELTA-N (H) 
			reg[rg] = data;
			delta_n = rate_adjust(((reg[0x11]<<8)|reg[0x10])<<GETA_BITS, sampleRate);
			break;

		case 0x12: // ENVELOP CONTROL 
			reg[0x12] = data;
			break;

		case 0x15: // DAC-DATA  (bit9-2)
		case 0x16: //           (bit1-0)
		case 0x17: //           (exponent)
			// TODO
			reg[rg] = data;
			break;

		case 0x18: // I/O-CONTROL (bit3-0)
			// TODO
			// 0 -> input
			// 1 -> output
			reg[0x18] = data;
			break;
		
		case 0x19: // I/O-DATA (bit3-0)
			// TODO
			reg[0x19] = data;
			break;

		case 0x1A: // PCM-DATA
			reg[0x1A] = data;
			break;

		case 0x00: // NOT USED 
		case 0x13: // NOT USED 
		case 0x14: // NOT USED 
		case 0x1B: // NOT USED 
		case 0x1C: // NOT USED 
		case 0x1D: // NOT USED 
		case 0x1E: // NOT USED 
		case 0x1F: // NOT USED 
		default:
			// do nothing
			break;
		}
	} else if ((0x20<=rg)&&(rg<0x40)) {
		int s = stbl[(rg-0x20)];
		if (s>=0) {
			slot[s]->patch.AM = (data>>7)&1;
			slot[s]->patch.PM = (data>>6)&1;
			slot[s]->patch.EG = (data>>5)&1;
			slot[s]->patch.KR = (data>>4)&1;
			slot[s]->patch.ML = (data)&15;
			slot[s]->UPDATE_ALL();
		}
		reg[rg] = data;
	} else if ((0x40<=rg)&&(rg<0x60)) {
		int s = stbl[(rg-0x40)];
		if (s>=0) {
			slot[s]->patch.KL = (data>>6)&3;
			slot[s]->patch.TL = (data)&63;
			slot[s]->UPDATE_ALL();
		}
		reg[rg] = data;
	} else if ((0x60<=rg)&&(rg<0x80)) {
		int s = stbl[(rg-0x60)];
		if (s>=0) {
			slot[s]->patch.AR = (data>>4)&15;
			slot[s]->patch.DR = (data)&15;
			slot[s]->UPDATE_EG();
		}
		reg[rg] = data;
	} else if ((0x80<=rg)&&(rg<0xA0)) {
		int s = stbl[(rg-0x80)];
		if (s>=0) {
			slot[s]->patch.SL = (data>>4)&15;
			slot[s]->patch.RR = (data)&15;
			slot[s]->UPDATE_EG();
		}
		reg[rg] = data;
	} else if ((0xA0<=rg)&&(rg<0xA9)) {
		int c = rg-0xA0;
		setFnumber(c, data + ((reg[rg+0x10]&3)<<8));
		ch[c].car.UPDATE_ALL();
		ch[c].mod.UPDATE_ALL();
		reg[rg] = data;
	} else if ((0xB0<=rg)&&(rg<0xB9)) {
		int c = rg-0xB0;
		setFnumber(c, ((data&3)<<8) + reg[rg-0x10]);
		setBlock(c, (data>>2)&7);
		if (((reg[rg]&0x20)==0)&&(data&0x20)) 
			keyOn(c); 
		else if((data&0x20)==0) 
			keyOff(c);
		ch[c].mod.UPDATE_ALL();
		ch[c].car.UPDATE_ALL();
		reg[rg] = data;
	} else if ((0xC0<=rg)&&(rg<0xC9)) {
		int c = rg-0xC0;
		slot[c*2]->patch.FB = (data>>1)&7;
		ch[c].alg = data&1;
		reg[rg] = data;
	} else if (rg==0xbd) {
		rythm_mode = data & 32;
		am_mode = (data>>7)&1;
		pm_mode = (data>>6)&1;
		reg[0xbd] = data;
	}
	checkMute();
}

byte Y8950::readReg(byte rg)
{
	return reg[rg];
}

byte Y8950::readStatus()
{
	return status;
}
