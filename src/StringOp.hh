// $Id$

#ifndef __STRINGOP_HH__
#define __STRINGOP_HH__

#include <string>
#include <sstream>
#include <strings.h>

namespace openmsx {

class StringOp
{
public:
	template <typename T>
	static std::string toString(const T& t);
	
	static int stringToInt(const std::string& str);
	static bool stringToBool(const std::string& str);
	static double stringToDouble(const std::string& str);
	
	static std::string toLower(const std::string& str);

	// case insensitive less then operator
	struct caseless {
		bool operator()(const std::string& s1, const std::string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
};

template<typename T>
std::string StringOp::toString(const T& t)
{
	std::ostringstream s;
	s << t;
	return s.str();
}

} // namespace openmsx

#endif
