//$Id$

#include <sstream>
#include <cstdlib>
#include "StringOp.hh"

using std::ostringstream;

namespace openmsx {

string StringOp::intToString(int num)
{
	ostringstream s;
	s << num;
	return s.str();
}

int StringOp::stringToInt(const string& str)
{
	return strtol(str.c_str(), NULL, 0);
}

} // namespace openmsx
