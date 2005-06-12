// $Id$

#ifndef STRINGOP_HH
#define STRINGOP_HH

#include <string>
#include <sstream>
#include <strings.h>

namespace openmsx {

namespace StringOp
{
	template <typename T> std::string toString(const T& t)
	{
		std::ostringstream s;
		s << t;
		return s.str();
	}

	template <typename T> std::string toHexString(const T& t)
	{
		std::ostringstream s;
		s << std::hex << t;
		return s.str();
	}

	int stringToInt(const std::string& str);
	bool stringToBool(const std::string& str);
	double stringToDouble(const std::string& str);

	std::string toLower(const std::string& str);

	// case insensitive less then operator
	struct caseless {
		bool operator()(const std::string& s1, const std::string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
}

} // namespace openmsx

#endif
