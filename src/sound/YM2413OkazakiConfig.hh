#ifndef YM2413OKAZAKICONFIG_HH
#define YM2413OKAZAKICONFIG_HH

// Number of bits in 'PhaseModulation' and 'EnvPhaseIndex' fixed point types.
static const int PM_FP_BITS =  8;
static const int EP_FP_BITS = 15;

// Dynamic range (Accuracy of sin table)
static const int DB_BITS = 8;
static const int DB_MUTE = 1 << DB_BITS;
static const int DBTABLEN = 3 * DB_MUTE; // enough to not have to check for overflow

static const double DB_STEP = 48.0 / DB_MUTE;
static const double EG_STEP = 0.375;
static const double TL_STEP = 0.75;

// PM speed(Hz) and depth(cent)
static const double PM_SPEED = 6.4;
static const double PM_DEPTH = 13.75;

// Size of Sintable ( 8 -- 18 can be used, but 9 recommended.)
static const int PG_BITS = 9;
static const int PG_WIDTH = 1 << PG_BITS;
static const int PG_MASK = PG_WIDTH - 1;

// Phase increment counter
static const int DP_BITS = 18;
static const int DP_BASE_BITS = DP_BITS - PG_BITS;

// Dynamic range of envelope
static const int EG_BITS = 7;

// Bits for linear value
static const int DB2LIN_AMP_BITS = 8;
static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;

// Bits for Pitch and Amp modulator
static const int PM_PG_BITS = 8;
static const int PM_PG_WIDTH = 1 << PM_PG_BITS;
static const int PM_DP_BITS = 16;
static const int PM_DP_WIDTH = 1 << PM_DP_BITS;
static const int PM_DP_MASK = PM_DP_WIDTH - 1;
static const int AM_PG_BITS = 8;
static const int AM_PG_WIDTH = 1 << AM_PG_BITS;
static const int AM_DP_BITS = 16;
static const int AM_DP_WIDTH = 1 << AM_DP_BITS;
static const int AM_DP_MASK = AM_DP_WIDTH - 1;

// LFO Amplitude Modulation table (verified on real YM3812)
// 27 output levels (triangle waveform);
// 1 level takes one of: 192, 256 or 448 samples
//
// Length: 210 elements.
//  Each of the elements has to be repeated
//  exactly 64 times (on 64 consecutive samples).
//  The whole table takes: 64 * 210 = 13440 samples.
//
// Verified on real YM3812 (OPL2), but I believe it's the same for YM2413
// because it closely matches the YM2413 AM parameters:
//    speed = 3.7Hz
//    depth = 4.875dB
// Also this approch can be easily implemented in HW, the previous one (see SVN
// history) could not.
static const unsigned LFO_AM_TAB_ELEMENTS = 210;

#endif
