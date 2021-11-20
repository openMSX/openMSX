#include "Date.hh"
#include <sstream>
#include <iomanip>

namespace openmsx::Date {

const char* const days[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

const char* const months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

template<bool FIRST, unsigned MUL, typename T>
[[nodiscard]] static constexpr bool parseDigit(unsigned c, T& t)
{
	c -= '0';
	if (c > 9) return false;
	if constexpr (FIRST) {
		t = c * MUL;
	} else {
		t += c * MUL;
	}
	return true;
}

time_t fromString(const char* p)
{
	struct tm tm{};

	// skip day
	p += 3;

	// space
	if (*p++ != ' ') return INVALID_TIME_T;

	// Parse month
	switch (*p++) {
	case 'J': // Jan Jun Jul
		switch (*p++) {
		case 'a': // Jan
			if (*p++ != 'n') return INVALID_TIME_T;
			tm.tm_mon = 0; break;
		case 'u': // Jun Jul
			switch (*p++) {
			case 'n': tm.tm_mon = 5; break;
			case 'l': tm.tm_mon = 6; break;
			default: return INVALID_TIME_T;
			}
			break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'F': // Feb
		if (*p++ != 'e') return INVALID_TIME_T;
		if (*p++ != 'b') return INVALID_TIME_T;
		tm.tm_mon = 1;
		break;
	case 'M': // Mar May
		if (*p++ != 'a') return INVALID_TIME_T;
		switch (*p++) {
		case 'r': tm.tm_mon = 2; break;
		case 'y': tm.tm_mon = 4; break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'A': // Apr Aug
		switch (*p++) {
		case 'p': // Apr
			if (*p++ != 'r') return INVALID_TIME_T;
			tm.tm_mon = 3; break;
		case 'u': // Aug
			if (*p++ != 'g') return INVALID_TIME_T;
			tm.tm_mon = 7; break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'S': // Sep
		if (*p++ != 'e') return INVALID_TIME_T;
		if (*p++ != 'p') return INVALID_TIME_T;
		tm.tm_mon = 8;
		break;
	case 'O': // Oct
		if (*p++ != 'c') return INVALID_TIME_T;
		if (*p++ != 't') return INVALID_TIME_T;
		tm.tm_mon = 9;
		break;
	case 'N': // Nov
		if (*p++ != 'o') return INVALID_TIME_T;
		if (*p++ != 'v') return INVALID_TIME_T;
		tm.tm_mon = 10;
		break;
	case 'D': // Dec
		if (*p++ != 'e') return INVALID_TIME_T;
		if (*p++ != 'c') return INVALID_TIME_T;
		tm.tm_mon = 11;
		break;
	default: return INVALID_TIME_T;
	}

	// space
	if (*p++ != ' ') return INVALID_TIME_T;

	// parse mday
	if (!parseDigit<true, 10>(*p++, tm.tm_mday)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(*p++, tm.tm_mday)) return INVALID_TIME_T;
	if ((tm.tm_mday < 1) || (31 < tm.tm_mday))   return INVALID_TIME_T;

	// space
	if (*p++ != ' ') return INVALID_TIME_T;

	// parse hour
	if (!parseDigit<true, 10>(*p++, tm.tm_hour)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(*p++, tm.tm_hour)) return INVALID_TIME_T;
	if ((tm.tm_hour < 0) || (23 < tm.tm_hour))   return INVALID_TIME_T;

	// colon
	if (*p++ != ':') return INVALID_TIME_T;

	// parse minute
	if (!parseDigit<true, 10>(*p++, tm.tm_min)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(*p++, tm.tm_min)) return INVALID_TIME_T;
	if ((tm.tm_min < 0) || (59 < tm.tm_min))    return INVALID_TIME_T;

	// colon
	if (*p++ != ':') return INVALID_TIME_T;

	// parse second
	if (!parseDigit<true, 10>(*p++, tm.tm_sec)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(*p++, tm.tm_sec)) return INVALID_TIME_T;
	if ((tm.tm_sec < 0) || (59 < tm.tm_sec))    return INVALID_TIME_T;

	// space
	if (*p++ != ' ') return INVALID_TIME_T;

	// parse year
	if (!parseDigit<true, 1000>(*p++, tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false, 100>(*p++, tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false,  10>(*p++, tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false,   1>(*p++, tm.tm_year)) return INVALID_TIME_T;
	tm.tm_year -= 1900;
	if (tm.tm_year < 0) return INVALID_TIME_T;

	tm.tm_isdst = -1;
	return mktime(&tm);
}

std::string toString(time_t time)
{
	if (time < 0) time = 0;
	struct tm* tm = localtime(&time);
	std::ostringstream sstr;
	sstr << std::setfill('0')
	     << days  [tm->tm_wday] << ' '
	     << months[tm->tm_mon]  << ' '
	     << std::setw(2) << tm->tm_mday << ' '
	     << std::setw(2) << tm->tm_hour << ':'
	     << std::setw(2) << tm->tm_min  << ':'
	     << std::setw(2) << tm->tm_sec  << ' '
	     << std::setw(4) << (tm->tm_year + 1900);
	return sstr.str();
}

} // namespace openmsx::Date
