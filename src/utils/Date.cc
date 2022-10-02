#include "Date.hh"
#include <array>
#include <concepts>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace openmsx::Date {

static constexpr std::array<std::string_view, 7> days = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static constexpr std::array<std::string_view, 12> months = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

template<bool FIRST, unsigned MUL>
[[nodiscard]] static constexpr bool parseDigit(unsigned c, std::integral auto& t)
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

time_t fromString(std::span<const char, 24> s)
{
	struct tm tm{};

	// skip day

	// space
	if (s[3] != ' ') return INVALID_TIME_T;

	// Parse month
	switch (s[4]) {
	case 'J': // Jan Jun Jul
		switch (s[5]) {
		case 'a': // Jan
			if (s[6] != 'n') return INVALID_TIME_T;
			tm.tm_mon = 0; break;
		case 'u': // Jun Jul
			switch (s[6]) {
			case 'n': tm.tm_mon = 5; break;
			case 'l': tm.tm_mon = 6; break;
			default: return INVALID_TIME_T;
			}
			break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'F': // Feb
		if (s[5] != 'e') return INVALID_TIME_T;
		if (s[6] != 'b') return INVALID_TIME_T;
		tm.tm_mon = 1;
		break;
	case 'M': // Mar May
		if (s[5] != 'a') return INVALID_TIME_T;
		switch (s[6]) {
		case 'r': tm.tm_mon = 2; break;
		case 'y': tm.tm_mon = 4; break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'A': // Apr Aug
		switch (s[5]) {
		case 'p': // Apr
			if (s[6] != 'r') return INVALID_TIME_T;
			tm.tm_mon = 3; break;
		case 'u': // Aug
			if (s[6] != 'g') return INVALID_TIME_T;
			tm.tm_mon = 7; break;
		default: return INVALID_TIME_T;
		}
		break;
	case 'S': // Sep
		if (s[5] != 'e') return INVALID_TIME_T;
		if (s[6] != 'p') return INVALID_TIME_T;
		tm.tm_mon = 8;
		break;
	case 'O': // Oct
		if (s[5] != 'c') return INVALID_TIME_T;
		if (s[6] != 't') return INVALID_TIME_T;
		tm.tm_mon = 9;
		break;
	case 'N': // Nov
		if (s[5] != 'o') return INVALID_TIME_T;
		if (s[6] != 'v') return INVALID_TIME_T;
		tm.tm_mon = 10;
		break;
	case 'D': // Dec
		if (s[5] != 'e') return INVALID_TIME_T;
		if (s[6] != 'c') return INVALID_TIME_T;
		tm.tm_mon = 11;
		break;
	default: return INVALID_TIME_T;
	}

	// space
	if (s[7] != ' ') return INVALID_TIME_T;

	// parse mday
	if (!parseDigit<true, 10>(s[8], tm.tm_mday)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(s[9], tm.tm_mday)) return INVALID_TIME_T;
	if ((tm.tm_mday < 1) || (31 < tm.tm_mday))   return INVALID_TIME_T;

	// space
	if (s[10] != ' ') return INVALID_TIME_T;

	// parse hour
	if (!parseDigit<true, 10>(s[11], tm.tm_hour)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(s[12], tm.tm_hour)) return INVALID_TIME_T;
	if ((tm.tm_hour < 0) || (23 < tm.tm_hour))   return INVALID_TIME_T;

	// colon
	if (s[13] != ':') return INVALID_TIME_T;

	// parse minute
	if (!parseDigit<true, 10>(s[14], tm.tm_min)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(s[15], tm.tm_min)) return INVALID_TIME_T;
	if ((tm.tm_min < 0) || (59 < tm.tm_min))    return INVALID_TIME_T;

	// colon
	if (s[16] != ':') return INVALID_TIME_T;

	// parse second
	if (!parseDigit<true, 10>(s[17], tm.tm_sec)) return INVALID_TIME_T;
	if (!parseDigit<false, 1>(s[18], tm.tm_sec)) return INVALID_TIME_T;
	if ((tm.tm_sec < 0) || (59 < tm.tm_sec))    return INVALID_TIME_T;

	// space
	if (s[19] != ' ') return INVALID_TIME_T;

	// parse year
	if (!parseDigit<true, 1000>(s[20], tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false, 100>(s[21], tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false,  10>(s[22], tm.tm_year)) return INVALID_TIME_T;
	if (!parseDigit<false,   1>(s[23], tm.tm_year)) return INVALID_TIME_T;
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
