#ifndef V9990DISPLAYTIMING_HH
#define V9990DISPLAYTIMING_HH

namespace openmsx {

/** A period, either horizontal or vertical, starts with a synchronisation
  * pulse followed by a blank period. Then border, display and border are
  * output follow. The final blank between the last border period and the
  * following sync signal is not included -- we don't need it.
  */
class V9990DisplayPeriod
{
public:
	const int cycle;
	const int blank;
	const int border1;
	const int display;
	const int border2;

	V9990DisplayPeriod(int cycle, int blank,
	                   int border1, int display, int border2);
};

class V9990DisplayTiming
{
public:
	/** The V9990 has an internal clock (MCLK @ 14MHz) and a terminal
	  * for an external clock (XTAL1), which can be connected to a 21
	  * or 25 MHz crystal. The Gfx9000 provides an 21 MHz crystal.
	  *
	  * The emulation combines these two clocks into one unified
	  * clock (UC) running at 42MHz - the smallest common multple
	  * of 14 and 21 MHz.
	  */
	static const int UC_TICKS_PER_SECOND = 3579545 * 12; // 42.9MHz

	/** The number of clockticks per line is independent of the crystal
	  * used or the display mode (NTSC/PAL)
	  */
	static const int UC_TICKS_PER_LINE = 2736;

	/** Horizontal (line) timing when using MCLK:
	  * 'Normal' display modes. Timing is in UC ticks.
	  */
	static const V9990DisplayPeriod lineMCLK;

	/** Horizontal (line) timing when using XTAL:
	  *'Overscan' modes without border. Timing is in UC ticks.
	  */
	static const V9990DisplayPeriod lineXTAL;

	/** NTSC display timing, when using MCLK:
	  * Normal display mode with borders. Timing is in display lines.
	  */
	static const V9990DisplayPeriod displayNTSC_MCLK;

	/** NTSC display timing, when using XTAL:
	  * Overscan mode without borders. Timing is in display lines.
	  */
	static const V9990DisplayPeriod displayNTSC_XTAL;

	/** PAL display timing, when using MCLK:
	  * Normal display mode with borders. Timing is in display lines.
	  */
	static const V9990DisplayPeriod displayPAL_MCLK;

	/** PAL display timing, when using XTAL:
	  * Overscan mode without borders. Timing is in display lines.
	  */
	static const V9990DisplayPeriod displayPAL_XTAL;

	/** Get the number of UC ticks in 1 frame.
	  * @param  palTiming  Use PAL timing? (False = NTSC timing)
	  * @return            The number of UC ticks in a frame
	  */
	static int getUCTicksPerFrame(bool palTiming);
};

} // openmsx

#endif
