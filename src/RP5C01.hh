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


class RP5C01
{
	public:
		RP5C01(bool emuMode);
		~RP5C01(); 
		
		void reset();
		nibble readPort(nibble port, const EmuTime &time);
		void writePort(nibble port, nibble value, const EmuTime &time);
	private:
		void initializeTime();
		void updateTimeRegs(const EmuTime &time);
		void regs2Time();
		void time2Regs();
		int daysInMonth(int month, int leapYear);
		void resetAlarm();

		static const int FREQ = 16384;
		
		static const nibble MODE_REG  = 13;
		static const nibble TEST_REG  = 14;
		static const nibble RESET_REG = 15;
		
		static const nibble TIME_BLOCK  = 0;
		static const nibble ALARM_BLOCK = 1;
		
		static const nibble MODE_BLOKSELECT =  0x3;
		static const nibble MODE_ALARMENABLE = 0x4;
		static const nibble MODE_TIMERENABLE = 0x8;

		static const nibble TEST_SECONDS = 0x1;
		static const nibble TEST_MINUTES = 0x2;
		static const nibble TEST_HOURS   = 0x4;
		static const nibble TEST_DAYS    = 0x8;
		
		static const nibble RESET_ALARM = 0x1;
		static const nibble RESET_FRACTION = 0x2;

		static const int daysInMonths[12]; 

		nibble modeReg, testReg, resetReg;
		nibble reg[4][13];
		static const nibble mask[4][13];

		bool emuTimeBased;
		EmuTime reference;
		int fraction;
		int seconds, minutes, hours;
		int dayWeek, days, months, years, leapYear;
		
};
#endif
