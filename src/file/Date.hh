// $Id$

#ifndef DATE_HH
#define DATE_HH

#include <string>
#include <time.h>

namespace openmsx {

class Date
{
public:
	static time_t fromString(const std::string& line);
	static std::string toString(time_t time);
};

} // namespace openmsx

#endif
