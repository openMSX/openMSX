#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;

// Constants
#include "YM2413OkazakiConfig.hh"


static void makeChecks()
{
	cout << "// This is a generated file. DO NOT EDIT!\n"
	        "\n"
	        "// These tables were generated for the following constants:\n"
	        "static_assert(PM_FP_BITS == " << PM_FP_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(EP_FP_BITS == " << EP_FP_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(DB_BITS == " << DB_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(DB_MUTE == " << DB_MUTE << ", \"mismatch, regenerate\");\n"
	        "static_assert(DBTABLEN == " << DBTABLEN << ", \"mismatch, regenerate\");\n"
	        "//static_assert(DB_STEP == " << DB_STEP << ", \"mismatch, regenerate\");\n"
	        "//static_assert(EG_STEP == " << EG_STEP << ", \"mismatch, regenerate\");\n"
	        "static_assert(PG_BITS == " << PG_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(PG_WIDTH == " << PG_WIDTH << ", \"mismatch, regenerate\");\n"
	        "static_assert(EG_BITS == " << EG_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(DB2LIN_AMP_BITS == " << DB2LIN_AMP_BITS << ", \"mismatch, regenerate\");\n"
	        "static_assert(LFO_AM_TAB_ELEMENTS == " << LFO_AM_TAB_ELEMENTS << ", \"mismatch, regenerate\");\n"
	        "\n";
}

template<typename T, unsigned N>
static void formatTable(const T (&tab)[N], unsigned columns, int width)
{
	assert((N % columns) == 0);
	for (unsigned i = 0; i < N; ++i) {
		if ((i % columns) == 0) cout << '\t';
		cout << setw(width) << tab[i]
		     << (((i % columns) != (columns - 1)) ? "," : ",\n");
	}
	cout << "};\n"
	        "\n";
}

template<typename T, unsigned M, unsigned N>
static void formatTable2(const T (&tab)[M][N], int width, int newLines, int columns)
{
	assert((N % columns) == 0);
	for (unsigned i = 0; i < M; ++i) {
		if (i && ((i % newLines) == 0)) cout << '\n';
		cout << "\t{ ";
		for (unsigned j = 0; j < N; ++j) {
			if (j && ((j % columns) == 0)) cout << "\n\t  ";
			cout << setw(width) << tab[i][j];
			if (j != (N - 1)) cout << ',';
		}
		cout << " },\n";
	}
	cout << "};\n"
	        "\n";
}

// Sawtooth function with amplitude 1 and period 1.
static inline float saw(float phase)
{
	if (phase < 0.25f) {
		return phase * 4.0f;
	} else if (phase < 0.75f) {
		return  2.0f - (phase * 4.0f);
	} else {
		return -4.0f + (phase * 4.0f);
	}
}

static void makePmTable()
{
	cout << "// LFO Phase Modulation table (copied from Burczynski core)\n"
	        "static const signed char pmTable[8][8] =\n"
	        "{\n"
	        "	{ 0, 0, 0, 0, 0, 0, 0, 0, }, // FNUM = 000xxxxxx\n"
	        "	{ 0, 0, 1, 0, 0, 0,-1, 0, }, // FNUM = 001xxxxxx\n"
	        "	{ 0, 1, 2, 1, 0,-1,-2,-1, }, // FNUM = 010xxxxxx\n"
	        "	{ 0, 1, 3, 1, 0,-1,-3,-1, }, // FNUM = 011xxxxxx\n"
	        "	{ 0, 2, 4, 2, 0,-2,-4,-2, }, // FNUM = 100xxxxxx\n"
	        "	{ 0, 2, 5, 2, 0,-2,-5,-2, }, // FNUM = 101xxxxxx\n"
	        "	{ 0, 3, 6, 3, 0,-3,-6,-3, }, // FNUM = 110xxxxxx\n"
	        "	{ 0, 3, 7, 3, 0,-3,-7,-3, }, // FNUM = 111xxxxxx\n"
	        "};\n"
	        "\n";
}

static void makeDB2LinTable()
{
	int dB2LinTab[2 * DBTABLEN];

	for (int i = 0; i < DB_MUTE; ++i) {
		dB2LinTab[i] = int(float((1 << DB2LIN_AMP_BITS) - 1) *
		                   powf(10, -float(i) * DB_STEP / 20));
	}
	dB2LinTab[DB_MUTE - 1] = 0;
	for (int i = DB_MUTE; i < DBTABLEN; ++i) {
		dB2LinTab[i] = 0;
	}
	for (int i = 0; i < DBTABLEN; ++i) {
		dB2LinTab[i + DBTABLEN] = -dB2LinTab[i];
	}

	cout << "// dB to linear table (used by Slot)\n"
	        "// dB(0 .. DB_MUTE-1) -> linear(0 .. DB2LIN_AMP_WIDTH)\n"
	        "// indices in range:\n"
	        "//   [0,        DB_MUTE )    actual values, from max to min\n"
	        "//   [DB_MUTE,  DBTABLEN)    filled with min val (to allow some overflow in index)\n"
	        "//   [DBTABLEN, 2*DBTABLEN)  as above but for negative output values\n"
	        "static int dB2LinTab[2 * DBTABLEN] = {\n";
	formatTable(dB2LinTab, 8, 5);
}

static void makeAdjustTable()
{
	unsigned AR_ADJUST_TABLE[1 << EG_BITS];
	AR_ADJUST_TABLE[0] = (1 << EG_BITS) - 1;
	for (int i = 1; i < (1 << EG_BITS); ++i) {
		AR_ADJUST_TABLE[i] = unsigned(float(1 << EG_BITS) - 1 -
		         ((1 << EG_BITS) - 1) * ::logf(float(i)) / ::logf(127.0f));
	}

	cout << "// Linear to Log curve conversion table (for Attack rate)\n"
	        "static unsigned AR_ADJUST_TABLE[1 << EG_BITS] = {\n";
	formatTable(AR_ADJUST_TABLE, 8, 4);
}

static void makeTllTable()
{
	// Processed version of Table III-5 from the Application Manual.
	static const unsigned kltable[16] = {
		0, 24, 32, 37, 40, 43, 45, 47, 48, 50, 51, 52, 53, 54, 55, 56
	};
	// Note: KL [0..3] results in {0.0, 1.5, 3.0, 6.0} dB/oct.
	// This is different from Y8950 and YMF262 which have {0, 3, 1.5, 6}.
	// (2nd and 3rd elements are swapped). Verified on real YM2413.

	unsigned tllTable[4][16 * 8];
	for (unsigned freq = 0; freq < 16 * 8; ++freq) {
		unsigned fnum = freq & 15;
		unsigned block = freq / 16;
		int tmp = 2 * kltable[fnum] - 16 * (7 - block);
		for (unsigned KL = 0; KL < 4; ++KL) {
			tllTable[KL][freq] = (tmp <= 0 || KL == 0) ? 0 : (tmp >> (3 - KL));
			assert(tllTable[KL][freq] <= 112);
		}
	}

	cout << "// KSL + TL Table   values are in range [0, 112]\n"
	     << "static byte tllTable[4][16 * 8] = {\n";
	formatTable2(tllTable, 3, 99, 16);
}

// lin(+0.0 .. +1.0) to dB(DB_MUTE-1 .. 0)
static int lin2db(float d)
{
	return (d == 0)
		? DB_MUTE - 1
		: std::min(-int(20.0f * log10f(d) / DB_STEP), DB_MUTE - 1); // 0 - 127
}

// Sin Table
static void makeSinTable()
{
	unsigned fullsintable[PG_WIDTH];
	unsigned halfsintable[PG_WIDTH];

	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		fullsintable[i] = lin2db(sinf(float(2.0 * M_PI) * i / PG_WIDTH));
	}
	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		fullsintable[PG_WIDTH / 2 - 1 - i] = fullsintable[i];
	}
	for (int i = 0; i < PG_WIDTH / 2; ++i) {
		fullsintable[PG_WIDTH / 2 + i] = DBTABLEN + fullsintable[i];
	}

	for (int i = 0; i < PG_WIDTH / 2; ++i) {
		halfsintable[i] = fullsintable[i];
	}
	for (int i = PG_WIDTH / 2; i < PG_WIDTH; ++i) {
		halfsintable[i] = fullsintable[0];
	}

	cout << "// WaveTable for each envelope amp\n"
	        "//  values are in range [0, DB_MUTE)             (for positive values)\n"
	        "//                  or  [0, DB_MUTE) + DBTABLEN  (for negative values)\n"
	        "static unsigned fullsintable[PG_WIDTH] = {\n";
	formatTable(fullsintable, 8, 5);
	cout << "static unsigned halfsintable[PG_WIDTH] = {\n";
	formatTable(halfsintable, 8, 5);
	cout << "static unsigned* waveform[2] = {fullsintable, halfsintable};\n"
	        "\n";
}

static void makeDphaseDRTable()
{
	int dphaseDRTable[16][16];
	for (unsigned Rks = 0; Rks < 16; ++Rks) {
		dphaseDRTable[Rks][0] = 0;
		for (unsigned DR = 1; DR < 16; ++DR) {
			unsigned RM = std::min(DR + (Rks >> 2), 15u);
			unsigned RL = Rks & 3;
			dphaseDRTable[Rks][DR] =
				((RL + 4) << EP_FP_BITS) >> (16 - RM);
		}
	}
	cout << "// Phase incr table for attack, decay and release\n"
	        "//  note: original code had indices swapped. It also had\n"
	        "//        a separate table for attack\n"
	        "//  17.15 fixed point\n"
	        "static int dphaseDRTable[16][16] = {\n";
	formatTable2(dphaseDRTable, 6, 999, 8);
}

static void makeLfoAmTable()
{
	cout << "// LFO Amplitude Modulation table (verified on real YM3812)\n"
	        "static const unsigned char lfo_am_table[LFO_AM_TAB_ELEMENTS] = {\n"
	        "	0,0,0,0,0,0,0,\n"
	        "	1,1,1,1,\n"
	        "	2,2,2,2,\n"
	        "	3,3,3,3,\n"
	        "	4,4,4,4,\n"
	        "	5,5,5,5,\n"
	        "	6,6,6,6,\n"
	        "	7,7,7,7,\n"
	        "	8,8,8,8,\n"
	        "	9,9,9,9,\n"
	        "	10,10,10,10,\n"
	        "	11,11,11,11,\n"
	        "	12,12,12,12,\n"
	        "	13,13,13,13,\n"
	        "	14,14,14,14,\n"
	        "	15,15,15,15,\n"
	        "	16,16,16,16,\n"
	        "	17,17,17,17,\n"
	        "	18,18,18,18,\n"
	        "	19,19,19,19,\n"
	        "	20,20,20,20,\n"
	        "	21,21,21,21,\n"
	        "	22,22,22,22,\n"
	        "	23,23,23,23,\n"
	        "	24,24,24,24,\n"
	        "	25,25,25,25,\n"
	        "	26,26,26,\n"
	        "	25,25,25,25,\n"
	        "	24,24,24,24,\n"
	        "	23,23,23,23,\n"
	        "	22,22,22,22,\n"
	        "	21,21,21,21,\n"
	        "	20,20,20,20,\n"
	        "	19,19,19,19,\n"
	        "	18,18,18,18,\n"
	        "	17,17,17,17,\n"
	        "	16,16,16,16,\n"
	        "	15,15,15,15,\n"
	        "	14,14,14,14,\n"
	        "	13,13,13,13,\n"
	        "	12,12,12,12,\n"
	        "	11,11,11,11,\n"
	        "	10,10,10,10,\n"
	        "	9,9,9,9,\n"
	        "	8,8,8,8,\n"
	        "	7,7,7,7,\n"
	        "	6,6,6,6,\n"
	        "	5,5,5,5,\n"
	        "	4,4,4,4,\n"
	        "	3,3,3,3,\n"
	        "	2,2,2,2,\n"
	        "	1,1,1,1,\n"
	        "};\n"
	        "\n";
}

static void makeSusLevTable()
{
	unsigned slTable[16];
	for (int i = 0; i < 16; ++i) {
		float x = (i == 15) ? 48.0f : (3.0f * i);
		slTable[i] = int(x / EG_STEP) << EP_FP_BITS;
	}

	cout << "// Sustain level (17.15 fixed point)\n"
	        "static const unsigned slTable[16] = {\n";
	formatTable(slTable, 4, 8);
}

static void makeMLTable()
{
	cout << "// ML-table\n"
	        "static const byte mlTable[16] = {\n"
	        "	1,   1*2,  2*2,  3*2,  4*2,  5*2,  6*2,  7*2,\n"
	        "	8*2, 9*2, 10*2, 10*2, 12*2, 12*2, 15*2, 15*2\n"
	        "};\n"
	        "\n";
}

int main()
{
	makeChecks();
	makePmTable();
	makeDB2LinTable();
	makeAdjustTable();
	makeTllTable();
	makeSinTable();
	makeDphaseDRTable();
	makeLfoAmTable();
	makeSusLevTable();
	makeMLTable();
}
