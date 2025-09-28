#include "RP5C01.hh"

#include "SRAM.hh"
#include "serialize.hh"

#include "narrow.hh"
#include "one_of.hh"

#include <array>
#include <cassert>
#include <ctime>

namespace openmsx {

// TODO  ALARM is not implemented        (not connected on MSX)
// TODO  1Hz 16Hz output not implemented (not connected on MSX)

static constexpr uint4_t MODE_REG  = 13;
static constexpr uint4_t TEST_REG  = 14;
static constexpr uint4_t RESET_REG = 15;

static constexpr uint4_t TIME_BLOCK  = 0;
static constexpr uint4_t ALARM_BLOCK = 1;

static constexpr uint4_t MODE_BLOK_SELECT  = 0x3;
static constexpr uint4_t MODE_ALARM_ENABLE = 0x4;
static constexpr uint4_t MODE_TIMER_ENABLE = 0x8;

static constexpr uint4_t TEST_SECONDS = 0x1;
static constexpr uint4_t TEST_MINUTES = 0x2;
static constexpr uint4_t TEST_DAYS    = 0x4;
static constexpr uint4_t TEST_YEARS   = 0x8;

static constexpr uint4_t RESET_ALARM    = 0x1;
static constexpr uint4_t RESET_FRACTION = 0x2;


// 0-bits are ignored on writing and return 0 on reading
static constexpr std::array mask = {
	std::array<uint4_t, 13>{0xf, 0x7, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0xf, 0x1, 0xf, 0xf},
	std::array<uint4_t, 13>{0x0, 0x0, 0xf, 0x7, 0xf, 0x3, 0x7, 0xf, 0x3, 0x0, 0x1, 0x3, 0x0},
	std::array<uint4_t, 13>{0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
	std::array<uint4_t, 13>{0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf},
};

RP5C01::RP5C01(CommandController& commandController, SRAM& regs_,
               EmuTime time, const std::string& name)
	: regs(regs_)
	, modeSetting(
		commandController,
		((name == "Real time clock") ? std::string_view("rtcmode") // bw-compat
		                             : tmpStrCat(name + " mode")),
		"Real Time Clock mode", RP5C01::RTCMode::EMUTIME,
		EnumSetting<RP5C01::RTCMode>::Map{
			{"EmuTime",  RP5C01::RTCMode::EMUTIME},
			{"RealTime", RP5C01::RTCMode::REALTIME}})
	, reference(time)
{
	initializeTime();
	reset(time);
}

void RP5C01::reset(EmuTime time)
{
	modeReg = MODE_TIMER_ENABLE;
	testReg = 0;
	resetReg = 0;
	updateTimeRegs(time);
}

uint4_t RP5C01::readPort(uint4_t port, EmuTime time)
{
	switch (port) {
	case MODE_REG:
	case TEST_REG:
	case RESET_REG:
		// nothing
		break;
	default:
		unsigned block = modeReg & MODE_BLOK_SELECT;
		if (block == one_of(TIME_BLOCK, ALARM_BLOCK)) {
			updateTimeRegs(time);
		}
	}
	return peekPort(port);
}

uint4_t RP5C01::peekPort(uint4_t port) const
{
	assert(port <= 0x0f);
	switch (port) {
	case MODE_REG:
		return modeReg;
	case TEST_REG:
	case RESET_REG:
		// write only
		return 0x0f; // TODO check this
	default:
		unsigned block = modeReg & MODE_BLOK_SELECT;
		uint4_t tmp = regs[block * 13 + port];
		return tmp & mask[block][port];
	}
}

void RP5C01::writePort(uint4_t port, uint4_t value, EmuTime time)
{
	assert (port <= 0x0f);
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
		if (value & RESET_ALARM) {
			resetAlarm();
		}
		if (value & RESET_FRACTION) {
			fraction = 0;
		}
		break;
	default:
		unsigned block = modeReg & MODE_BLOK_SELECT;
		if (block == one_of(TIME_BLOCK, ALARM_BLOCK)) {
			updateTimeRegs(time);
		}
		regs.write(block * 13 + port, value & mask[block][port]);
		if (block == one_of(TIME_BLOCK, ALARM_BLOCK)) {
			regs2Time();
		}
	}
}

void RP5C01::initializeTime()
{
	time_t t = time(nullptr);
	const struct tm* tm = localtime(&t);
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
	unsigned hours_ = hours;
	if (!regs[ALARM_BLOCK * 13 + 10]) {
		// 12 hours mode
		if (hours >= 12) hours_ = (hours - 12) + 20;
	}

	regs.write(TIME_BLOCK  * 13 +  0, narrow<uint8_t>( seconds   % 10));
	regs.write(TIME_BLOCK  * 13 +  1, narrow<uint8_t>( seconds   / 10));
	regs.write(TIME_BLOCK  * 13 +  2, narrow<uint8_t>( minutes   % 10));
	regs.write(TIME_BLOCK  * 13 +  3, narrow<uint8_t>( minutes   / 10));
	regs.write(TIME_BLOCK  * 13 +  4, narrow<uint8_t>( hours_    % 10));
	regs.write(TIME_BLOCK  * 13 +  5, narrow<uint8_t>( hours_    / 10));
	regs.write(TIME_BLOCK  * 13 +  6, narrow<uint8_t>( dayWeek));
	regs.write(TIME_BLOCK  * 13 +  7, narrow<uint8_t>((days+1)   % 10)); // 0-30 -> 1-31
	regs.write(TIME_BLOCK  * 13 +  8, narrow<uint8_t>((days+1)   / 10)); // 0-11 -> 1-12
	regs.write(TIME_BLOCK  * 13 +  9, narrow<uint8_t>((months+1) % 10));
	regs.write(TIME_BLOCK  * 13 + 10, narrow<uint8_t>((months+1) / 10));
	regs.write(TIME_BLOCK  * 13 + 11, narrow<uint8_t>( years     % 10));
	regs.write(TIME_BLOCK  * 13 + 12, narrow<uint8_t>( years     / 10));
	regs.write(ALARM_BLOCK * 13 + 11, narrow<uint8_t>( leapYear));
}

static constexpr int daysInMonth(int month, unsigned leapYear)
{
	constexpr std::array<uint8_t, 12> daysInMonths = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	month %= 12;
	return ((month == 1) && (leapYear == 0)) ? 29 : daysInMonths[month];
}

void RP5C01::updateTimeRegs(EmuTime time)
{
	if (modeSetting.getEnum() == RTCMode::EMUTIME) {
		// sync with EmuTime, perfect emulation
		auto elapsed = reference.getTicksTill(time);
		reference.advance(time);

		// in test mode increase sec/min/.. at a rate of 16384Hz
		fraction += (modeReg & MODE_TIMER_ENABLE) ? elapsed : 0;
		unsigned carrySeconds = (testReg & TEST_SECONDS)
		                      ? elapsed : fraction / FREQ;
		seconds  += carrySeconds;
		unsigned carryMinutes = (testReg & TEST_MINUTES)
		                      ? elapsed : seconds / 60;
		minutes  += carryMinutes;
		hours    += minutes / 60;
		unsigned carryDays = (testReg & TEST_DAYS)
		                   ? elapsed : hours / 24;
		if (carryDays) {
			// Only correct for number of days in a month when we
			// actually advance the day. Because otherwise e.g. the
			// following scenario goes wrong:
			// - Suppose current date is 'xx/07/31' and we want to
			//   change that to 'xx/12/31'.
			// - Changing the months is done in two steps: first the
			//   lower then the higher nibble.
			// - So temporary we go via the (invalid) date 'xx/02/31'
			//   (february does not have 32 days)
			// - We must NOT roll over the days and advance to march.
			days     += narrow_cast<int>(carryDays);
			dayWeek  += carryDays;
			while (days >= daysInMonth(months, leapYear)) {
				// TODO not correct because leapYear is not updated
				//      is only triggered when we update several months
				//      at a time (but might happen in TEST_DAY mode)
				days -= daysInMonth(months, leapYear);
				months++;
			}
		}
		unsigned carryYears = (testReg & TEST_YEARS)
		                    ? elapsed : unsigned(months / 12);
		years    += carryYears;
		leapYear += carryYears;

		fraction %= FREQ;
		seconds  %= 60;
		minutes  %= 60;
		hours    %= 24;
		dayWeek  %= 7;
		months   %= 12;
		years    %= 100;
		leapYear %= 4;

		time2Regs();
	} else {
		// sync with host clock
		//   writes to time, test and reset registers have no effect
		initializeTime();
	}
}

void RP5C01::resetAlarm()
{
	for (auto i : xrange(2, 9)) {
		regs.write(ALARM_BLOCK * 13 + i, 0);
	}
}

template<typename Archive>
void RP5C01::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("reference", reference,
	             "fraction",  fraction,
	             "seconds",   seconds,
	             "minutes",   minutes,
	             "hours",     hours,
	             "dayWeek",   dayWeek,
	             "years",     years,
	             "leapYear",  leapYear,
	             "days",      days,
	             "months",    months,
	             "modeReg",   modeReg,
	             "testReg",   testReg,
	             "resetReg",  resetReg);
}
INSTANTIATE_SERIALIZE_METHODS(RP5C01);

} // namespace openmsx
