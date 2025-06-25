#include "VDPAccessSlots.hh"

#include <array>
#include <utility>

namespace openmsx::VDPAccessSlots {

// These tables must contain at least one value that is bigger or equal
// to 1368+136. So we extend the data with some cyclic duplicates.

// Screen rendering disabled (or vertical border).
// This is correct (measured on real V9938) for bitmap and character mode.
// TODO also correct for text mode? See 'vdp-timing-2.html for more details.
static constexpr std::array<int16_t, 154 + 17> slotsScreenOff = {
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

// Bitmap mode, sprites disabled.
static constexpr std::array<int16_t, 88 + 16> slotsSpritesOff = {
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

// Bitmap mode, sprites enabled.
static constexpr std::array<int16_t, 31 + 3> slotsSpritesOn = {
	  28,   92,  162,  170,  188,  220,  252,  316,  348,  380,
	 444,  476,  508,  572,  604,  636,  700,  732,  764,  828,
	 860,  892,  956,  988, 1020, 1084, 1116, 1148, 1212, 1264,
	1330,
	1368+28, 1368+92, 1368+162,
};

// Character mode, sprites enabled.
static constexpr std::array<int16_t, 31 + 3> slotsChar = {
	  32,   96,  166,  174,  188,  220,  252,  316,  348,  380,
	 444,  476,  508,  572,  604,  636,  700,  732,  764,  828,
	 860,  892,  956,  988, 1020, 1084, 1116, 1148, 1212, 1268,
	1334,
	1368+32, 1368+96, 1368+166,
};

// Text mode.
static constexpr std::array<int16_t, 47 + 10> slotsText = {
	   2,   10,   18,   26,   34,   42,   50,   58,   66,  166,
	 174,  182,  190,  198,  206,  214,  222,  312,  408,  504,
	 600,  696,  792,  888,  984, 1080, 1176, 1206, 1214, 1222,
	1230, 1238, 1246, 1254, 1262, 1270, 1278, 1286, 1294, 1302,
	1310, 1318, 1326, 1336, 1346, 1354, 1362,
	1368+ 2, 1368+10, 1368+18, 1368+26, 1368+ 34,
	1368+42, 1368+50, 1368+58, 1368+66, 1368+166,
};


// TMS9918 (MSX1) cycle numbers translated to V99x8 cycles (multiplied by 4).
// MSX1 screen off.
static constexpr std::array<int16_t, 107 + 18> slotsMsx1ScreenOff = {
	   4,   12,   20,   28,   36,   44,   52,   60,   68,   76,
	  84,   92,  100,  108,  116,  124,  132,  140,  148,  156,
	 164,  172,  180,  188,  196,  204,  220,  236,  252,  268,
	 284,  300,  316,  332,  348,  364,  380,  396,  412,  428,
	 444,  460,  476,  492,  508,  524,  540,  556,  572,  588,
	 604,  620,  636,  652,  668,  684,  700,  716,  732,  748,
	 764,  780,  796,  812,  828,  844,  860,  876,  892,  908,
	 924,  940,  956,  972,  988, 1004, 1020, 1036, 1052, 1068,
	1084, 1100, 1116, 1132, 1148, 1164, 1180, 1196, 1212, 1228,
	1236, 1244, 1252, 1260, 1268, 1276, 1284, 1292, 1300, 1308,
	1316, 1324, 1332, 1340, 1348, 1356, 1364,
	1368+  4, 1368+ 12, 1368+ 20, 1368+ 28, 1368+ 36,
	1368+ 44, 1368+ 52, 1368+ 60, 1368+ 68, 1368+ 76,
	1368+ 84, 1368+ 92, 1368+100, 1368+108, 1368+116,
	1368+124, 1368+132, 1368+140,
};

// MSX1 graphic mode 1 and 2 (aka screen 1 and 2).
static constexpr std::array<int16_t, 19 + 8> slotsMsx1Gfx12 = {
	   4,   12,   20,   28,  116,  124,  132,  140,  220,  348,
	 476,  604,  732,  860,  988, 1116, 1236, 1244, 1364,
	1368+  4, 1368+ 12, 1368+ 20, 1368+ 28, 1368+116,
	1368+124, 1368+132, 1368+140,
};

// MSX1 graphic mode 3 (aka screen 3).
static constexpr std::array<int16_t, 51 + 8> slotsMsx1Gfx3 = {
	   4,   12,   20,   28,  116,  124,  132,  140,  220,  228,
	 260,  292,  324,  348,  356,  388,  420,  452,  476,  484,
	 516,  548,  580,  604,  612,  644,  676,  708,  732,  740,
	 772,  804,  836,  860,  868,  900,  932,  964,  988,  996,
	1028, 1060, 1092, 1116, 1124, 1156, 1188, 1220, 1236, 1244,
	1364,
	1368+  4, 1368+ 12, 1368+ 20, 1368+ 28, 1368+116,
	1368+124, 1368+132, 1368+140,
};


// MSX1 text mode 1 (aka screen 0 width 40).
static constexpr std::array<int16_t, 91 + 18> slotsMsx1Text = {
	   4,   12,   20,   28,   36,   44,   52,   60,   68,   76,
	  84,   92,  100,  108,  116,  124,  132,  140,  148,  156,
	 164,  172,  180,  188,  196,  204,  212,  220,  228,  244,
	 268,  292,  316,  340,  364,  388,  412,  436,  460,  484,
	 508,  532,  556,  580,  604,  628,  652,  676,  700,  724,
	 748,  772,  796,  820,  844,  868,  892,  916,  940,  964,
	 988, 1012, 1036, 1060, 1084, 1108, 1132, 1156, 1180, 1196,
	1204, 1212, 1220, 1228, 1236, 1244, 1252, 1260, 1268, 1276,
	1284, 1292, 1300, 1308, 1316, 1324, 1332, 1340, 1348, 1356,
	1364,
	1368+  4, 1368+ 12, 1368+ 20, 1368+ 28, 1368+ 36,
	1368+ 44, 1368+ 52, 1368+ 60, 1368+ 68, 1368+ 76,
	1368+ 84, 1368+ 92, 1368+100, 1368+108, 1368+116,
	1368+124, 1368+132, 1368+140,
};

// Helper functions to transform the above tables into a format that is easier
// (=faster) to work with.

struct AccessTable
{
	operator std::span<const uint8_t, NUM_DELTAS * TICKS>() const { return values; }

protected:
	std::array<uint8_t, NUM_DELTAS * TICKS> values = {};
};

struct CycleTable : AccessTable
{
	constexpr CycleTable(bool msx1, std::span<const int16_t> slots)
	{
		// !!! Keep this in sync with the 'Delta' enum !!!
		constexpr std::array<int, NUM_DELTAS> delta = {
			0, 1, 16, 24, 28, 32, 40, 48, 64, 72, 88, 104, 120, 128, 136
		};

		size_t out = 0;
		for (auto step : delta) {
			int p = 0;
			while (slots[p] < step) ++p;
			for (auto i : xrange(TICKS)) {
				if ((slots[p] - i) < step) ++p;
				assert((slots[p] - i) >= step);
				unsigned t = slots[p] - i;
				if (msx1) {
					if (step <= 40) assert(t < 256);
				} else {
					assert(t < 256);
				}
				values[out++] = narrow_cast<uint8_t>(t);
			}
		}
	}
};

struct ZeroTable : AccessTable
{
};

static constexpr CycleTable tabSpritesOn     (false, slotsSpritesOn);
static constexpr CycleTable tabSpritesOff    (false, slotsSpritesOff);
static constexpr CycleTable tabChar          (false, slotsChar);
static constexpr CycleTable tabText          (false, slotsText);
static constexpr CycleTable tabScreenOff     (false, slotsScreenOff);
static constexpr CycleTable tabMsx1Gfx12     (true,  slotsMsx1Gfx12);
static constexpr CycleTable tabMsx1Gfx3      (true,  slotsMsx1Gfx3);
static constexpr CycleTable tabMsx1Text      (true,  slotsMsx1Text);
static constexpr CycleTable tabMsx1ScreenOff (true,  slotsMsx1ScreenOff);
static constexpr ZeroTable  tabBroken;


[[nodiscard]] static inline std::span<const uint8_t, NUM_DELTAS * TICKS> getTab(const VDP& vdp)
{
	if (vdp.getBrokenCmdTiming()) return tabBroken;
	bool enabled = vdp.isDisplayEnabled();
	bool sprites = vdp.spritesEnabledRegister();
	auto mode    = vdp.getDisplayMode();
	bool bitmap  = mode.isBitmapMode();
	bool text    = mode.isTextMode();
	bool gfx3    = mode.getBase() == DisplayMode::GRAPHIC3;

	if (vdp.isMSX1VDP()) {
		if (!enabled) return tabMsx1ScreenOff;
		return text ? tabMsx1Text
		            : (gfx3 ? tabMsx1Gfx3
		                    : tabMsx1Gfx12);
		// TODO undocumented modes
	} else {
		if (bitmap) {
			return !enabled ? tabScreenOff
			      : sprites ? tabSpritesOn
			                : tabSpritesOff;
		} else {
			// 'enabled' or 'sprites' doesn't matter in V99x8 non-bitmap mode
			// See: https://github.com/openMSX/openMSX/issues/1754
			return text ? tabText
			            : tabChar;
		}
	}
}

EmuTime getAccessSlot(
	EmuTime frame_, EmuTime time, Delta delta,
	const VDP& vdp)
{
	VDP::VDPClock frame(frame_);
	unsigned ticks = frame.getTicksTill_fast(time) % TICKS;
	auto tab = getTab(vdp);
	return time + VDP::VDPClock::duration(tab[std::to_underlying(delta) + ticks]);
}

Calculator getCalculator(
	EmuTime frame, EmuTime time, EmuTime limit,
	const VDP& vdp)
{
	auto tab = getTab(vdp);
	return {frame, time, limit, tab};
}

} // namespace openmsx::VDPAccessSlots
