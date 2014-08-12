#include "VDPAccessSlots.hh"

namespace openmsx {
namespace VDPAccessSlots {

// TODO the following 3 tables are correct for bitmap screen modes,
//      still need to investigate character and text modes.
// These tables must contain at least one value that is bigger or equal
// to 1368+136. So we extend the data with some cyclic duplicates.
static const int16_t slotsScreenOff[154 + 17] = {
	   0,    8,   16,   24,   32,   40,   48,   56,   64,   72,
	  80,   88,   96,  104,  112,  120,  164,  172,  180,  188,
	 196,  204,  212,  220,  228,  236,  244,  252,  260,  268,
	 276,  292,  300,  308,  316,  324,  332,  340,  348,  356,
	 364,  372,  380,  388,  396,  404,  420,  428,  436,  444,
	 452,  460,  468,  476,  484,  492,  500,  508,  516,  524,
	 532,  548,  556,  564,  572,  580,  588,  596,  604,  612,
	 620,  628,  636,  644,  652,  660,  676,  684,  692,  700,
	 708,  716,  724,  732,  740,  748,  756,  764,  772,  780,
	 788,  804,  812,  820,  828,  836,  844,  852,  860,  868,
	 876,  884,  892,  900,  908,  916,  932,  940,  948,  956,
	 964,  972,  980,  988,  996, 1004, 1012, 1020, 1028, 1036,
	1044, 1060, 1068, 1076, 1084, 1092, 1100, 1108, 1116, 1124,
	1132, 1140, 1148, 1156, 1164, 1172, 1188, 1196, 1204, 1212,
	1220, 1228, 1268, 1276, 1284, 1292, 1300, 1308, 1316, 1324,
	1334, 1344, 1352, 1360,
	1368+  0, 1368+  8, 1368+16, 1368+ 24, 1368+ 32,
	1368+ 40, 1368+ 48, 1368+56, 1368+ 64, 1368+ 72,
	1368+ 80, 1368+ 88, 1368+96, 1368+104, 1368+112,
	1368+120, 1368+164
};

static const int16_t slotsSpritesOff[88 + 16] = {
	   6,   14,   22,   30,   38,   46,   54,   62,   70,   78,
	  86,   94,  102,  110,  118,  162,  170,  182,  188,  214,
	 220,  246,  252,  278,  310,  316,  342,  348,  374,  380,
	 406,  438,  444,  470,  476,  502,  508,  534,  566,  572,
	 598,  604,  630,  636,  662,  694,  700,  726,  732,  758,
	 764,  790,  822,  828,  854,  860,  886,  892,  918,  950,
	 956,  982,  988, 1014, 1020, 1046, 1078, 1084, 1110, 1116,
	1142, 1148, 1174, 1206, 1212, 1266, 1274, 1282, 1290, 1298,
	1306, 1314, 1322, 1332, 1342, 1350, 1358, 1366,
	1368+  6, 1368+14, 1368+ 22, 1368+ 30, 1368+ 38,
	1368+ 46, 1368+54, 1368+ 62, 1368+ 70, 1368+ 78,
	1368+ 86, 1368+94, 1368+102, 1368+110, 1368+118,
	1368+162,
};

static const int16_t slotsSpritesOn[31 + 3] = {
	  28,   92,  162,  170,  188,  220,  252,  316,  348,  380,
	 444,  476,  508,  572,  604,  636,  700,  732,  764,  828,
	 860,  892,  956,  988, 1020, 1084, 1116, 1148, 1212, 1264,
	1330,
	1368+28, 1368+92, 1368+162,
};

static uint8_t tabSpritesOn [NUM_DELTAS * TICKS];
static uint8_t tabSpritesOff[NUM_DELTAS * TICKS];
static uint8_t tabScreenOff [NUM_DELTAS * TICKS];
static uint8_t tabBroken    [NUM_DELTAS * TICKS];

static void initTable(const int16_t* slots, uint8_t* output)
{
	// !!! Keep this in sync with the 'Delta' enum !!!
	static const int delta[NUM_DELTAS] = {
		0, 1, 16, 24, 32, 40, 48, 64, 72, 88, 104, 120, 128, 136
	};

	for (auto step : delta) {
		int p = 0;
		while (slots[p] < step) ++p;
		for (int i = 0; i < TICKS; ++i) {
			if ((slots[p] - i) < step) ++p;
			assert((slots[p] - i) >= step);
			unsigned t = slots[p] - i;
			assert(t < 256);
			*output++ = t;
		}
	}
}
void initTables()
{
	static bool init = false;
	if (init) return;
	init = true;

	initTable(slotsSpritesOn,  tabSpritesOn);
	initTable(slotsSpritesOff, tabSpritesOff);
	initTable(slotsScreenOff,  tabScreenOff);
	for (int i = 0; i < NUM_DELTAS * TICKS; ++i) {
		tabBroken[i] = 0;
	}
}

static const uint8_t* getTab(bool display, bool sprites, bool broken)
{
	return broken ? tabBroken
	              : (display ? (sprites ? tabSpritesOn : tabSpritesOff)
	                         : tabScreenOff);
}

EmuTime getAccessSlot(
	EmuTime::param frame_, EmuTime::param time, Delta delta,
	bool display, bool sprites, bool broken)
{
	VDP::VDPClock frame(frame_);
	unsigned ticks = frame.getTicksTill_fast(time) % TICKS;
	auto* tab = getTab(display, sprites, broken);
	return time + VDP::VDPClock::duration(tab[delta + ticks]);
}

Calculator getCalculator(
	EmuTime::param frame, EmuTime::param time, EmuTime::param limit,
	bool display, bool sprites, bool broken)
{
	auto* tab = getTab(display, sprites, broken);
	return Calculator(frame, time, limit, tab);
}

} // namespace VDPAccessSlots
} // namespace openmsx
