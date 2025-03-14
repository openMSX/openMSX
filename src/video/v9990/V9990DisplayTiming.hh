#ifndef V9990DISPLAYTIMING_HH
#define V9990DISPLAYTIMING_HH

namespace openmsx {

/** A period, either horizontal or vertical, starts with a synchronisation
  * pulse followed by a blank period. Then border, display and border are
  * output follow. The final blank between the last border period and the
  * following sync signal is not included -- we don't need it.
  */
struct V9990DisplayPeriod
{
	int cycle;
	int blank;
	int border1;
	int display;
	int border2;
};

class V9990DisplayTiming
{
public:
	/** The V9990 has an internal clock (MCLK @ 14MHz) and a terminal
	  * for an external clock (XTAL1), which can be connected to a 21
	  * or 25 MHz crystal. The Gfx9000 provides an 21 MHz crystal.
	  *
	  * The emulation combines these two clocks into one unified
	  * clock (UC) running at 42MHz - the smallest common multiple
	  * of 14 and 21 MHz.
	  */
	static constexpr int UC_TICKS_PER_SECOND = 3579545 * 12; // 42.9MHz

	/** The number of clock ticks per line is independent of the crystal
	  * used or the display mode (NTSC/PAL)
	  */
	static constexpr int UC_TICKS_PER_LINE = 2736;

	/** Horizontal (line) timing when using MCLK:
	  * 'Normal' display modes. Timing is in UC ticks.
	  */
	static constexpr auto lineMCLK = V9990DisplayPeriod{
		.cycle = V9990DisplayTiming::UC_TICKS_PER_LINE, .blank = 400, .border1 = 112, .display = 2048, .border2 = 112};

	/** Horizontal (line) timing when using XTAL:
	  *'Overscan' modes without border. Timing is in UC ticks.
	  */
	static constexpr auto lineXTAL = V9990DisplayPeriod{
		.cycle = V9990DisplayTiming::UC_TICKS_PER_LINE, .blank = 372, .border1 = 0, .display = 2304, .border2 = 0};

	/** NTSC display timing, when using MCLK:
	  * Normal display mode with borders. Timing is in display lines.
	  */
	static constexpr auto displayNTSC_MCLK = V9990DisplayPeriod{
		.cycle = 262, .blank = 15, .border1 = 14, .display = 212, .border2 = 14};

	/** NTSC display timing, when using XTAL:
	  * Overscan mode without borders. Timing is in display lines.
	  */
	static constexpr auto displayNTSC_XTAL = V9990DisplayPeriod{
		.cycle = 262, .blank = 15, .border1 = 0, .display = 240, .border2 = 0};

	/** PAL display timing, when using MCLK:
	  * Normal display mode with borders. Timing is in display lines.
	  */
	static constexpr auto displayPAL_MCLK = V9990DisplayPeriod{
		.cycle = 313, .blank = 15, .border1 = 41, .display = 212, .border2 = 37};

	/** PAL display timing, when using XTAL:
	  * Overscan mode without borders. Timing is in display lines.
	  */
	static constexpr auto displayPAL_XTAL = V9990DisplayPeriod{
		.cycle = 313, .blank = 15, .border1 = 0, .display = 290, .border2 = 0};


	/** Get the number of UC ticks in 1 frame.
	  * @param  palTiming  Use PAL timing? (False = NTSC timing)
	  * @return            The number of UC ticks in a frame
	  */
	[[nodiscard]] static constexpr int getUCTicksPerFrame(bool palTiming) {
		return palTiming ? (displayPAL_MCLK.cycle  * UC_TICKS_PER_LINE)
		                 : (displayNTSC_MCLK.cycle * UC_TICKS_PER_LINE);
	}
};

} // openmsx

#endif
