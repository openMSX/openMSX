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
	        "STATIC_ASSERT(PM_FP_BITS == " << PM_FP_BITS << ");\n"
	        "STATIC_ASSERT(EP_FP_BITS == " << EP_FP_BITS << ");\n"
	        "STATIC_ASSERT(DB_BITS == " << DB_BITS << ");\n"
	        "STATIC_ASSERT(DB_MUTE == " << DB_MUTE << ");\n"
	        "STATIC_ASSERT(DBTABLEN == " << DBTABLEN << ");\n"
	        "//STATIC_ASSERT(DB_STEP == " << DB_STEP << ");\n"
	        "//STATIC_ASSERT(EG_STEP == " << EG_STEP << ");\n"
	        "//STATIC_ASSERT(PM_DEPTH == " << PM_DEPTH << ");\n"
	        "STATIC_ASSERT(PG_BITS == " << PG_BITS << ");\n"
	        "STATIC_ASSERT(PG_WIDTH == " << PG_WIDTH << ");\n"
	        "STATIC_ASSERT(EG_BITS == " << EG_BITS << ");\n"
	        "STATIC_ASSERT(DB2LIN_AMP_BITS == " << DB2LIN_AMP_BITS << ");\n"
	        "STATIC_ASSERT(PM_PG_BITS == " << PM_PG_BITS << ");\n"
	        "STATIC_ASSERT(PM_PG_WIDTH == " << PM_PG_WIDTH << ");\n"
	        "STATIC_ASSERT(LFO_AM_TAB_ELEMENTS == " << LFO_AM_TAB_ELEMENTS << ");\n"
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
static inline double saw(double phase)
{
	if (phase < 0.25) {
		return phase * 4.0;
	} else if (phase < 0.75) {
		return 2.0 - (phase * 4.0);
	} else {
		return -4.0 + (phase * 4.0);
	}
}

static void makePmTable()
{
	int pmtable[PM_PG_WIDTH];

	for (int i = 0; i < PM_PG_WIDTH; ++i) {
		double t = pow(2, PM_DEPTH / 1200.0 * saw(i / double(PM_PG_WIDTH)));
		pmtable[i] = lrint(t * (1 << PM_FP_BITS));
	}

	cout << "// Table for Pitch Modulator (24.8 fixed point)\n"
	        "static int pmtable[PM_PG_WIDTH] = {\n";
	formatTable(pmtable, 8, 4);
}

static void makeDB2LinTable()
{
	int dB2LinTab[2 * DBTABLEN];

	for (int i = 0; i < DB_MUTE; ++i) {
		dB2LinTab[i] = int(double((1 << DB2LIN_AMP_BITS) - 1) *
		                   pow(10, -double(i) * DB_STEP / 20));
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
		AR_ADJUST_TABLE[i] = unsigned(double(1 << EG_BITS) - 1 -
		         ((1 << EG_BITS) - 1) * ::log(double(i)) / ::log(127.0));
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

	unsigned tllTable[16 * 8][4];
	for (unsigned freq = 0; freq < 16 * 8; ++freq) {
		unsigned fnum = freq & 15;
		unsigned block = freq / 16;
		int tmp = 2 * kltable[fnum] - 16 * (7 - block);
		for (unsigned KL = 0; KL < 4; ++KL) {
			tllTable[freq][KL] = (tmp <= 0 || KL == 0) ? 0 : (tmp >> (3 - KL));
		}
	}

	cout << "// KSL + TL Table   values are in range [0, 112)\n"
	     << "static unsigned tllTable[16 * 8][4] = {\n";
	formatTable2(tllTable, 4, 16, 4);
}

// lin(+0.0 .. +1.0) to dB(DB_MUTE-1 .. 0)
static int lin2db(double d)
{
	return (d == 0)
		? DB_MUTE - 1
		: std::min(-int(20.0 * log10(d) / DB_STEP), DB_MUTE - 1); // 0 - 127
}

// Sin Table
static void makeSinTable()
{
	unsigned fullsintable[PG_WIDTH];
	unsigned halfsintable[PG_WIDTH];

	for (int i = 0; i < PG_WIDTH / 4; ++i) {
		fullsintable[i] = lin2db(sin(2.0 * M_PI * i / PG_WIDTH));
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
	int SL[16];
	for (int i = 0; i < 16; ++i) {
		double x = (i == 15) ? 48.0 : (3.0 * i);
		SL[i] = int(x / EG_STEP) << EP_FP_BITS;
	}

	cout << "// Sustain level (17.15 fixed point)\n"
	        "static const int SL[16] = {\n";
	formatTable(SL, 4, 8);
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
}
