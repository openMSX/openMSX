// $Id$

#ifndef __STRINGOP_HH__
#define __STRINGOP_HH__

#include <string>
#include <sstream>

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
