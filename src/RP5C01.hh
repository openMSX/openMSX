// $Id$
//
// This class implements the RP5C01 chip (RTC)
// 
// * For techncal details on RP5C01 see
//     http://w3.qahwah.net/joost/openMSX/RP5C01.pdf
//  


#ifndef __RP5C01_HH__
#define __RP5C01_HH__

#include "openmsx.hh"
#include "EmuTime.hh"


namespace openmsx {

class RP5C01
{
public:
	RP5C01(bool emuMode, byte* data, const EmuTime &time);
	~RP5C01(); 
	
	void reset(const EmuTime &time);
	nibble readPort(nibble port, const EmuTime &time);
	void writePort(nibble port, nibble value, const EmuTime &time);
	
private:
	void initializeTime();
	void updateTimeRegs(const EmuTime &time);
	void regs2Time();
	void time2Regs();
	void resetAlarm();

	static const int FREQ = 16384;

	nibble modeReg, testReg, resetReg;
	byte* reg;

	bool emuTimeBased;
	Clock<FREQ> reference;
	int fraction;
	int seconds, minutes, hours;
	int dayWeek, days, months, years, leapYear;
};

} // namespace openmsx
#endif
