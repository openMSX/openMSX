// $Id$

#ifndef DATE_HH
#define DATE_HH

#include <string>
#include <time.h>

namespace openmsx {

namespace Date {

	time_t fromString(const std::string& line);
	std::string toString(time_t time);

} // namespace Date

} // namespace openmsx

#endif
