// $Id$

#ifndef __STRINGOP_HH__
#define __STRINGOP_HH__

#include <string>
#include <sstream>
#include <strings.h>

using std::string;
using std::ostringstream;

namespace openmsx {

class StringOp
{
public:
	template <typename T>
	static string toString(const T& t);
	
	static int stringToInt(const string& str);
	static bool stringToBool(const string& str);
	
	static string toLower(const string& str);

	// case insensitive less then operator
	struct caseless {
		bool operator()(const string& s1, const string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
};

template<typename T>
string StringOp::toString(const T& t)
{
	ostringstream s;
	s << t;
	return s.str();
}

} // namespace openmsx

#endif
