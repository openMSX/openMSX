// $Id$

#include "RP5C01.hh"
#include <cassert>
#include <time.h>

//TODO  ALARM is not implemented (not connected on MSX)
//TODO  1Hz 16Hz output not implemented (not connected on MSX)

//TODO  Test register is ignored


RP5C01::RP5C01()
{
	//TODO load saved state
	
	// if no saved state found
	for (int b=0; b<4; b++) {
		for (int r=0; r<13; r++) {
			reg[b][r] = 0;
		}
	}
	reg[ALARM_BLOCK][10] = 1;	// set 24hour mode   TODO check

	initializeTime();
}

RP5C01::~RP5C01()
{
	//TODO save state
}

void RP5C01::reset()
{
	writePort(MODE_REG, MODE_TIMERENABLE);
	writePort(TEST_REG,  0);
	writePort(RESET_REG, 0);
}

nibble RP5C01::readPort(nibble port)
{
	assert (port<=0x0f);
	switch (port) {
	case MODE_REG:
		return modeReg;
	case TEST_REG:
	case RESET_REG:
		// write only
		return 0x0f;	// TODO check this
	default:
		int block = modeReg & MODE_BLOKSELECT;
		if (block==TIME_BLOCK)
			updateTimeRegs();
		nibble tmp = reg[block][port];
		return tmp & mask[block][port];
	}
}

void RP5C01::writePort(nibble port, nibble value)
{
	assert (port<=0x0f);
	switch (port) {
	case MODE_REG:
		updateTimeRegs();
		modeReg = value;
		break;
	case TEST_REG:
		updateTimeRegs();
		testReg = value;
		break;
	case RESET_REG:
		resetReg = value;
		if (value & RESET_ALARM)
			resetAlarm();
		if (value & RESET_FRACTION)
			fraction = 0;
		break;
	default:
		int block = modeReg & MODE_BLOKSELECT;
		if (block==TIME_BLOCK)
			updateTimeRegs();
		reg[block][port] = value & mask[block][port];
		if (block==TIME_BLOCK)
			regs2Time();
	}
}

// 0-bits are ignored on writing and return 0 on reading
const nibble RP5C01::mask[4][13] = {
	{ 0xf, 0x7, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0xf, 0x1, 0xf, 0xf},
	{ 0x0, 0x0, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0x0, 0x1, 0x3, 0x0},
	{ 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
	{ 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf}
};


void RP5C01::initializeTime()
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	seconds  = tm->tm_sec;
	minutes  = tm->tm_min;
	hours    = tm->tm_hour;
	dayWeek  = tm->tm_wday;	// 0=sunday	TODO check this
	days     = tm->tm_mday;	// 0-30		TODO check this
	months   = tm->tm_mon;
	years    = tm->tm_year - 1980;	// TODO check this
	leapYear = tm->tm_year % 4;
	time2Regs();

	reference = SDL_GetTicks();
	fraction = 0;	// fractions of a second
}

void RP5C01::regs2Time()
{
	seconds  = reg[TIME_BLOCK][ 0]+10*reg[TIME_BLOCK][ 1];
	minutes  = reg[TIME_BLOCK][ 2]+10*reg[TIME_BLOCK][ 3];
	hours    = reg[TIME_BLOCK][ 4]+10*reg[TIME_BLOCK][ 5];
	dayWeek  = reg[TIME_BLOCK][ 6];
	days     = reg[TIME_BLOCK][ 7]+10*reg[TIME_BLOCK][ 8];
	months   = reg[TIME_BLOCK][ 9]+10*reg[TIME_BLOCK][10];
	years    = reg[TIME_BLOCK][11]+10*reg[TIME_BLOCK][12];
	leapYear = reg[ALARM_BLOCK][11];

	if (!reg[ALARM_BLOCK][10]) {
		// 12 hours mode
		if (hours >= 20) hours = (hours - 20) + 12;
	}
}
	
void RP5C01::time2Regs()
{
	if (!reg[ALARM_BLOCK][10]) {
		// 12 hours mode
		if (hours >= 12) hours = (hours - 12) + 20;
	}
	
	reg[TIME_BLOCK][ 0]  = seconds % 10;
	reg[TIME_BLOCK][ 1]  = seconds / 10;
	reg[TIME_BLOCK][ 2]  = minutes % 10;
	reg[TIME_BLOCK][ 3]  = minutes / 10;
	reg[TIME_BLOCK][ 4]  = hours   % 10;
	reg[TIME_BLOCK][ 5]  = hours   / 10;
	reg[TIME_BLOCK][ 6]  = dayWeek;
	reg[TIME_BLOCK][ 7]  = days    % 10;
	reg[TIME_BLOCK][ 8]  = days    / 10;
	reg[TIME_BLOCK][ 9]  = months  % 10;
	reg[TIME_BLOCK][10]  = months  / 10;
	reg[TIME_BLOCK][11]  = years   % 10;
	reg[TIME_BLOCK][12]  = years   / 10;
	reg[ALARM_BLOCK][11] = leapYear;
}

void RP5C01::updateTimeRegs()
{
	if (modeReg & MODE_TIMERENABLE) {
		Uint32 now = SDL_GetTicks();	// current time in microseconds
		Uint32 elapsed = (now-reference)+fraction;
		reference = now;
		
		fraction =  elapsed % 1000;
		seconds  += elapsed/1000; 
		minutes  += seconds/60;     seconds %= 60;
		hours    += minutes/60;     minutes %= 60;
		int carryDays = hours/24; 
		days     += carryDays;      hours   %= 24;
		dayWeek = (dayWeek + carryDays) % 7;
		while (days >= daysInMonth(months, leapYear)) {
			days -= daysInMonth(months, leapYear);
			months++;
		}
		int carryYears = months/12;
		years = (years + carryYears) % 100; months %= 12;
		leapYear = (leapYear + carryYears) % 4;
		
		time2Regs();
	}
}

const int RP5C01::daysInMonths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int RP5C01::daysInMonth(int month, int leapYear) 
{
	if ((month == 1) && (leapYear == 0))
		return 29;
	return daysInMonths[month];
}
 
void RP5C01::resetAlarm()
{
	for (int i=2; i<=8; i++) {
		reg[ALARM_BLOCK][i]=0;
	}
}

