// $Id$

#ifndef __STRINGOP_HH__
#define __STRINGOP_HH__

#include <string>

using std::string;

namespace openmsx {

class StringOp
{
public:
	static string intToString(int num);
	static int stringToInt(const string& str);

	static bool stringToBool(const string& str);
	
	static string toLower(const string& str);
};

} // namespace openmsx

#endif
