// $Id$

#ifndef __DATE_HH__
#define __DATE_HH__

#include <string>
#include <time.h>

using std::string;

namespace openmsx {

class Date
{
public:
	static time_t fromString(const string& line);
	static string toString(time_t time);
};

} // namespace openmsx

#endif
