#include "V9990DisplayTiming.hh"

namespace openmsx {

/* Horizontal display timing: MCLK or XTAL1 */
const V9990DisplayPeriod V9990DisplayTiming::lineMCLK = V9990DisplayPeriod(
	V9990DisplayTiming::UC_TICKS_PER_LINE, 400, 112, 2048, 112 );
const V9990DisplayPeriod V9990DisplayTiming::lineXTAL = V9990DisplayPeriod(
	V9990DisplayTiming::UC_TICKS_PER_LINE, 372,   0, 2304,   0 );

/* Vertical display timing: NTSC or PAL */
const V9990DisplayPeriod V9990DisplayTiming::displayNTSC_MCLK =
	V9990DisplayPeriod(262, 15, 14, 212, 14);
const V9990DisplayPeriod V9990DisplayTiming::displayNTSC_XTAL =
	V9990DisplayPeriod(262, 15,  0, 240,  0);
const V9990DisplayPeriod V9990DisplayTiming::displayPAL_MCLK  =
	V9990DisplayPeriod(313, 15, 41, 212, 37);
const V9990DisplayPeriod V9990DisplayTiming::displayPAL_XTAL  =
	V9990DisplayPeriod(313, 15,  0, 290,   0);


V9990DisplayPeriod::V9990DisplayPeriod(
		int cycle_, int blank_, int border1_, int display_, int border2_)
	: cycle(cycle_), blank(blank_), border1(border1_)
	, display(display_), border2(border2_)
{
}

int V9990DisplayTiming::getUCTicksPerFrame(bool palTiming)
{
	return palTiming ? (displayPAL_MCLK.cycle  * UC_TICKS_PER_LINE)
	                 : (displayNTSC_MCLK.cycle * UC_TICKS_PER_LINE);
}

} // openmsx
