
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
		void updateTime();
		int daysInMonth(int month, int leapYear);
		void resetAlarm();

		static const nibble MODE_REG  = 13;
		static const nibble TEST_REG  = 14;
		static const nibble RESET_REG = 15;
		static const nibble TIME_BLOCK  = 0;
		static const nibble ALARM_BLOCK = 1;
		static const int daysInMonths[]; 

		nibble modeReg;
		nibble testReg;
		nibble resetReg;
		nibble reg[13][4];

		Uint32 reference;
		int fraction;
};
#endif
