// $Id$

#ifndef __RP5C01_HH__
#define __RP5C01_HH__

#include "openmsx.hh"
#include <SDL/SDL.h>


class RP5C01
{
	public:
		RP5C01();
		~RP5C01(); 
		
		void reset();
		nibble readPort(nibble port);
		void writePort(nibble port, nibble value);
	private:
		void initializeTime();
		void updateTimeRegs();
		void regs2Time();
		void time2Regs();
		int daysInMonth(int month, int leapYear);
		void resetAlarm();

		static const nibble MODE_REG  = 13;
		static const nibble TEST_REG  = 14;
		static const nibble RESET_REG = 15;
		
		static const nibble TIME_BLOCK  = 0;
		static const nibble ALARM_BLOCK = 1;
		
		static const nibble MODE_BLOKSELECT =  0x3;
		static const nibble MODE_ALARMENABLE = 0x4;
		static const nibble MODE_TIMERENABLE = 0x8;

		static const nibble RESET_ALARM = 0x1;
		static const nibble RESET_FRACTION = 0x2;

		static const int daysInMonths[12]; 

		nibble modeReg, testReg, resetReg;
		nibble reg[4][13];
		static const nibble mask[4][13];

		Uint32 reference;
		int fraction;
		int seconds, minutes, hours;
		int dayWeek, days, months, years, leapYear;
		
};
#endif
