
#include "RP5C01.hh"
#include <assert.h>
#include <time.h>


//TODO  AM/PM mode not implemented (not necessary for MSX)


RP5C01::RP5C01()
{
	//TODO load saved state
	initializeTime();
}

RP5C01::~RP5C01()
{
	//TODO save state
}

void RP5C01::reset()
{
	//TODO check this
	writePort(MODE_REG,  4);
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
		// write only
		return 0x0f;	// TODO check this
	case RESET_REG:
		// write only
		return 0x0f;	// TODO check this
	default:
		int block = modeReg&3;
		if (block==TIME_BLOCK)
			updateTime();
		return reg[port][block];
	}
}

void RP5C01::writePort(nibble port, nibble value)
{
	assert (port<=0x0f);
	switch (port) {
	case MODE_REG:
		updateTime();
		modeReg = value;
		break;
	case TEST_REG:
		updateTime();
		testReg = value;
		break;
	case RESET_REG:
		resetReg = value;
		if (value&1)
			resetAlarm();
		if (value&2)
			fraction = 0;
		break;
	default:
		int block = modeReg&3;
		if (block==TIME_BLOCK)
			updateTime();
		reg[port][block] = value;
	}
}

void RP5C01::initializeTime()
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	reg[ 0][TIME_BLOCK] = tm->tm_sec  % 10;
	reg[ 1][TIME_BLOCK] = tm->tm_sec  / 10;
	reg[ 2][TIME_BLOCK] = tm->tm_min  % 10;
	reg[ 3][TIME_BLOCK] = tm->tm_min  / 10;
	reg[ 4][TIME_BLOCK] = tm->tm_hour % 10;
	reg[ 5][TIME_BLOCK] = tm->tm_hour / 10;
	reg[ 6][TIME_BLOCK] = tm->tm_wday;
	reg[ 7][TIME_BLOCK] = tm->tm_mday % 10;
	reg[ 8][TIME_BLOCK] = tm->tm_mday / 10;
	reg[ 9][TIME_BLOCK] = tm->tm_mon  % 10;
	reg[10][TIME_BLOCK] = tm->tm_mon  / 10;
	reg[11][TIME_BLOCK] = tm->tm_year % 10;
	reg[12][TIME_BLOCK] = tm->tm_year / 10;

	reg[10][ALARM_BLOCK] = 0x0f;	// 24 hour mode
	reg[11][ALARM_BLOCK] = tm->tm_year % 4;	// leap year counter

	reference = SDL_GetTicks();
	fraction = 0;
}

void RP5C01::updateTime()
{
	Uint32 now = SDL_GetTicks();	// current time in microseconds
	Uint32 elapsed = (now-reference)+fraction;
	fraction = elapsed % 1000;
	reference = now;
	
	int seconds = reg[ 0][TIME_BLOCK]+10*reg[ 1][TIME_BLOCK];
	int minutes = reg[ 2][TIME_BLOCK]+10*reg[ 3][TIME_BLOCK];
	int hours   = reg[ 4][TIME_BLOCK]+10*reg[ 5][TIME_BLOCK];
	int dayWeek = reg[ 6][TIME_BLOCK];
	int days    = reg[ 7][TIME_BLOCK]+10*reg[ 8][TIME_BLOCK];
	int months  = reg[ 9][TIME_BLOCK]+10*reg[10][TIME_BLOCK];
	int years   = reg[11][TIME_BLOCK]+10*reg[12][TIME_BLOCK];
	int leapYear = reg[11][ALARM_BLOCK];
	
	seconds += elapsed/1000; 
	minutes += seconds/60; seconds %= 60;
	hours += minutes/60; minutes %= 60;
	int carryDays = hours/24; days += carryDays; hours %= 24;
	dayWeek = (dayWeek + carryDays) % 7;
	while (days >= daysInMonth(months, leapYear)) {
		days -= daysInMonth(months, leapYear);
		months++;
	}
	int carryYears = months/12; years = (years + carryYears) % 100; months %= 12;
	leapYear = (leapYear + carryYears) % 4;
	
	reg[ 0][TIME_BLOCK] = seconds % 10;
	reg[ 1][TIME_BLOCK] = seconds / 10;
	reg[ 2][TIME_BLOCK] = minutes % 10;
	reg[ 3][TIME_BLOCK] = minutes / 10;
	reg[ 4][TIME_BLOCK] = hours   % 10;
	reg[ 5][TIME_BLOCK] = hours   / 10;
	reg[ 6][TIME_BLOCK] = dayWeek;
	reg[ 7][TIME_BLOCK] = days    % 10;
	reg[ 8][TIME_BLOCK] = days    / 10;
	reg[ 9][TIME_BLOCK] = months  % 10;
	reg[10][TIME_BLOCK] = months  / 10;
	reg[11][TIME_BLOCK] = years   % 10;
	reg[12][TIME_BLOCK] = years   / 10;
	reg[11][ALARM_BLOCK] = leapYear;
}

const int RP5C01::daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int RP5C01::daysInMonth(int month, int leapYear) 
{
	if ((month == 1) && (leapYear == 0))
		return 29;
	return daysInMonths[month];
}
 
void RP5C01::resetAlarm()
{
	for (int i=2; i<=8; i++) {
		reg[i][ALARM_BLOCK]=0;
	}
}

