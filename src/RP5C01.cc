// $Id$

#include "RP5C01.hh"
#include "SettingsConfig.hh"
#include "EnumSetting.hh"
#include "SRAM.hh"
#include <cassert>
#include <ctime>
#include <string>

namespace openmsx {

//TODO  ALARM is not implemented        (not connected on MSX)
//TODO  1Hz 16Hz output not implemented (not connected on MSX)

const nibble MODE_REG  = 13;
const nibble TEST_REG  = 14;
const nibble RESET_REG = 15;

const nibble TIME_BLOCK  = 0;
const nibble ALARM_BLOCK = 1;

const nibble MODE_BLOKSELECT  = 0x3;
const nibble MODE_ALARMENABLE = 0x4;
const nibble MODE_TIMERENABLE = 0x8;

const nibble TEST_SECONDS = 0x1;
const nibble TEST_MINUTES = 0x2;
const nibble TEST_HOURS   = 0x4;
const nibble TEST_DAYS    = 0x8;

const nibble RESET_ALARM    = 0x1;
const nibble RESET_FRACTION = 0x2;


// 0-bits are ignored on writing and return 0 on reading
static const nibble mask[4][13] = {
	{ 0xf, 0x7, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0xf, 0x1, 0xf, 0xf},
	{ 0x0, 0x0, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0x0, 0x1, 0x3, 0x0},
	{ 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
	{ 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf}
};


RP5C01::RP5C01(SRAM& regs_, const EmuTime& time)
	: regs(regs_), reference(time)
{
	EnumSetting<RTCMode>::Map modeMap;
	modeMap["EmuTime"] = EMUTIME;
	modeMap["RealTime"] = REALTIME;
	modeSetting.reset(new EnumSetting<RTCMode>(
		"rtcmode", "Real Time Clock mode", EMUTIME, modeMap));

	initializeTime();
	reset(time);
}

RP5C01::~RP5C01()
{
}

void RP5C01::reset(const EmuTime& time)
{
	modeReg = MODE_TIMERENABLE;
	testReg = 0;
	resetReg = 0;
	updateTimeRegs(time);
}

nibble RP5C01::readPort(nibble port, const EmuTime& time)
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
			if (block == TIME_BLOCK)
				updateTimeRegs(time);
			nibble tmp = regs[block * 13 + port];
			return tmp & mask[block][port];
	}
}

void RP5C01::writePort(nibble port, nibble value, const EmuTime& time)
{
	assert (port<=0x0f);
	switch (port) {
		case MODE_REG:
			updateTimeRegs(time);
			modeReg = value;
			break;
		case TEST_REG:
			updateTimeRegs(time);
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
			if (block == TIME_BLOCK)
				updateTimeRegs(time);
			regs.write(block * 13 + port, value & mask[block][port]);
			if (block == TIME_BLOCK)
				regs2Time();
	}
}

void RP5C01::initializeTime()
{
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	fraction = 0;			// fractions of a second
	seconds  = tm->tm_sec;		// 0-59
	minutes  = tm->tm_min;		// 0-59
	hours    = tm->tm_hour;		// 0-23
	dayWeek  = tm->tm_wday;		// 0-6   0=sunday
	days     = tm->tm_mday-1;	// 0-30
	months   = tm->tm_mon;		// 0-11
	years    = tm->tm_year - 80;	// 0-99  0=1980
	leapYear = tm->tm_year % 4;	// 0-3   0=leap year
	time2Regs();
}

void RP5C01::regs2Time()
{
	seconds  = regs[TIME_BLOCK * 13 + 0] + 10 * regs[TIME_BLOCK * 13 + 1];
	minutes  = regs[TIME_BLOCK * 13 + 2] + 10 * regs[TIME_BLOCK * 13 + 3];
	hours    = regs[TIME_BLOCK * 13 + 4] + 10 * regs[TIME_BLOCK * 13 + 5];
	dayWeek  = regs[TIME_BLOCK * 13 + 6];
	days     = regs[TIME_BLOCK * 13 + 7] + 10 * regs[TIME_BLOCK * 13 + 8] - 1;
	months   = regs[TIME_BLOCK * 13 + 9] + 10 * regs[TIME_BLOCK * 13 +10] - 1;
	years    = regs[TIME_BLOCK * 13 +11] + 10 * regs[TIME_BLOCK * 13 +12];
	leapYear = regs[ALARM_BLOCK * 13 +11];

	if (!regs[ALARM_BLOCK * 13 + 10]) {
		// 12 hours mode
		if (hours >= 20) hours = (hours - 20) + 12;
	}
}

void RP5C01::time2Regs()
{
	int hours_ = hours;
	if (!regs[ALARM_BLOCK * 13 + 10]) {
		// 12 hours mode
		if (hours >= 12) hours_ = (hours - 12) + 20;
	}

	regs.write(TIME_BLOCK  * 13 +  0,  seconds   % 10);
	regs.write(TIME_BLOCK  * 13 +  1,  seconds   / 10);
	regs.write(TIME_BLOCK  * 13 +  2,  minutes   % 10);
	regs.write(TIME_BLOCK  * 13 +  3,  minutes   / 10);
	regs.write(TIME_BLOCK  * 13 +  4,  hours_    % 10);
	regs.write(TIME_BLOCK  * 13 +  5,  hours_    / 10);
	regs.write(TIME_BLOCK  * 13 +  6,  dayWeek);
	regs.write(TIME_BLOCK  * 13 +  7, (days+1)   % 10);	// 0-30 -> 1-31
	regs.write(TIME_BLOCK  * 13 +  8, (days+1)   / 10);	// 0-11 -> 1-12
	regs.write(TIME_BLOCK  * 13 +  9, (months+1) % 10);
	regs.write(TIME_BLOCK  * 13 + 10, (months+1) / 10);
	regs.write(TIME_BLOCK  * 13 + 11,  years     % 10);
	regs.write(TIME_BLOCK  * 13 + 12,  years     / 10);
	regs.write(ALARM_BLOCK * 13 + 11,  leapYear);
}

static int daysInMonth(int month, int leapYear)
{
	const int daysInMonths[12] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	month %= 12;
	return ((month == 1) && (leapYear == 0)) ? 29
	                                         : daysInMonths[month];
}

void RP5C01::updateTimeRegs(const EmuTime& time)
{
	if (modeSetting->getValue() == EMUTIME) {
		// sync with EmuTime, perfect emulation
		uint64 elapsed = (modeReg & MODE_TIMERENABLE)
			? reference.getTicksTill(time)
			: 0;
		reference.advance(time);

		// in test mode increase sec/min/.. at a rate of 16384Hz
		uint64 testSeconds = (testReg & TEST_SECONDS) ? elapsed : 0;
		uint64 testMinutes = (testReg & TEST_MINUTES) ? elapsed : 0;
		uint64 testHours   = (testReg & TEST_HOURS  ) ? elapsed : 0;
		uint64 testDays    = (testReg & TEST_DAYS   ) ? elapsed : 0;

		fraction += elapsed;
		seconds  += fraction/FREQ  + testSeconds; fraction %= FREQ;
		minutes  += seconds / 60   + testMinutes; seconds  %= 60;
		hours    += minutes / 60   + testHours;   minutes  %= 60;
		int carryDays = hours / 24 + testDays;
		days     += carryDays;      hours   %= 24;
		dayWeek = (dayWeek + carryDays) % 7;
		while (days >= daysInMonth(months, leapYear)) {
			days -= daysInMonth(months, leapYear);
			months++;
		}
		int carryYears = months / 12;
		years = (years + carryYears) % 100; months %= 12;
		leapYear = (leapYear + carryYears) % 4;

		time2Regs();
	} else {
		// sync with host clock
		//   writes to time, test and reset registers have no effect
		initializeTime();
	}
}

void RP5C01::resetAlarm()
{
	for (int i = 2; i <= 8; i++) {
		regs.write(ALARM_BLOCK * 13 + i, 0);
	}
}

} // namespace openmsx
