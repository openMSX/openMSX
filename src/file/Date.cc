// $Id$

#include <sstream>
#include <iomanip>
#include <cstdio>
#include "Date.hh"

using std::ostringstream;
using std::setfill;
using std::setw;

namespace openmsx {

const char* const days[7] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat" ,"Sun"
};

const char* const months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

time_t Date::fromString(const string& line)
{
	char day[4];
	char month[4];
	struct tm tm;
	int items = sscanf(line.c_str(), "%3s %3s %2u %2u:%2u:%2u %4u",
	                   day, month, &tm.tm_mday, &tm.tm_hour,
	                   &tm.tm_min, &tm.tm_sec, &tm.tm_year);
	tm.tm_year -= 1900;
	tm.tm_isdst = -1;
	tm.tm_mon = -1;
	for (int i = 0; i < 12; ++i) {
		if (strcmp(month, months[i]) == 0) {
			tm.tm_mon = i;
			break;
		}
	}
	if ((items != 9) || (tm.tm_mon == -1)) {
		return static_cast<time_t>(-1);
	}
	return mktime(&tm);
}

string Date::toString(time_t time)
{
	struct tm* tm;
	tm = localtime(&time);
	ostringstream sstr;
	sstr << setfill('0')
	     << days[tm->tm_wday] << " "
	     << months[tm->tm_mon] << " "
	     << setw(2) << tm->tm_mday << " "
	     << setw(2) << tm->tm_hour << ":"
	     << setw(2) << tm->tm_min << ":"
	     << setw(2) << tm->tm_sec << " "
	     << setw(4) << (tm->tm_year + 1900);
	return sstr.str();
}

} // namespace openmsx
