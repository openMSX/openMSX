// $Id$

#ifndef __DATE_HH__
#define __DATE_HH__

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
