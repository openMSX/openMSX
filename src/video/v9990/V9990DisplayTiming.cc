// $Id$

#include "V9990DisplayTiming.hh"

namespace openmsx {

/* Horizontal display timing: MCLK or XTAL1 */
const V9990DisplayPeriod V9990DisplayTiming::lineMCLK = V9990DisplayPeriod(
	V9990DisplayTiming::UC_TICKS_PER_LINE, 400, 112, 2048, 112 );
const V9990DisplayPeriod V9990DisplayTiming::lineXTAL = V9990DisplayPeriod(
	V9990DisplayTiming::UC_TICKS_PER_LINE, 372,   0, 2304,   0 );

/* Vertical display timing: NTSC or PAL */
const V9990DisplayPeriod V9990DisplayTiming::displayNTSC_MCLK =
	V9990DisplayPeriod(262, 18, 14, 212, 14);
const V9990DisplayPeriod V9990DisplayTiming::displayNTSC_XTAL =
	V9990DisplayPeriod(262, 18,  0, 240,  0);
const V9990DisplayPeriod V9990DisplayTiming::displayPAL_MCLK  =
	V9990DisplayPeriod(313, 18, 41, 212, 37);
const V9990DisplayPeriod V9990DisplayTiming::displayPAL_XTAL  =
	V9990DisplayPeriod(313, 18,  0, 290,   0);

V9990DisplayPeriod::V9990DisplayPeriod(int cycle, int blank, int border1, int display, int border2)
{
	this->cycle   = cycle;
	this->blank   = blank;
	this->border1 = border1;
	this->display = display;
	this->border2 = border2;
}

int V9990DisplayTiming::getUCTicksPerFrame(bool palTiming) {
	return palTiming? (displayPAL_MCLK.cycle  * UC_TICKS_PER_LINE)
	                : (displayNTSC_MCLK.cycle * UC_TICKS_PER_LINE);
}
} // openmsx
