// $Id$

#include "Date.hh"
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace openmsx {

namespace Date {

const char* const days[7] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat" ,"Sun"
};

const char* const months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

time_t fromString(const std::string& line)
{
	char day[4];
	char month[4];
	struct tm tm;
	int items = sscanf(line.c_str(), "%3s %3s %2d %2d:%2d:%2d %4d",
	                   day, month, &tm.tm_mday, &tm.tm_hour,
	                   &tm.tm_min, &tm.tm_sec, &tm.tm_year);
	tm.tm_year -= 1900;
	tm.tm_isdst = -1;
	tm.tm_mon = -1;
	if ((tm.tm_sec  < 0) || (59 < tm.tm_sec)  ||
	    (tm.tm_min  < 0) || (59 < tm.tm_min)  ||
	    (tm.tm_hour < 0) || (23 < tm.tm_hour) ||
	    (tm.tm_mday < 1) || (31 < tm.tm_mday) ||
	    (tm.tm_year < 0)) {
		return static_cast<time_t>(-1);
	}
	for (int i = 0; i < 12; ++i) {
		if (strcmp(month, months[i]) == 0) {
			tm.tm_mon = i;
			break;
		}
	}
	if ((items != 7) || (tm.tm_mon == -1)) {
		return static_cast<time_t>(-1);
	}
	return mktime(&tm);
}

std::string toString(time_t time)
{
	struct tm* tm;
	tm = localtime(&time);
	std::ostringstream sstr;
	sstr << std::setfill('0')
	     << days[tm->tm_wday] << " "
	     << months[tm->tm_mon] << " "
	     << std::setw(2) << tm->tm_mday << " "
	     << std::setw(2) << tm->tm_hour << ":"
	     << std::setw(2) << tm->tm_min << ":"
	     << std::setw(2) << tm->tm_sec << " "
	     << std::setw(4) << (tm->tm_year + 1900);
	return sstr.str();
}

} // namespace Date

} // namespace openmsx
