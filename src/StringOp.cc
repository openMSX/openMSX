//$Id$

#include <cstdlib>
#include <algorithm>
#include "StringOp.hh"

using std::transform;

namespace openmsx {

int StringOp::stringToInt(const string& str)
{
	return strtol(str.c_str(), NULL, 0);
}

bool StringOp::stringToBool(const string& str)
{
	string low = StringOp::toLower(str);
	return (low == "true") || (low == "yes") || (low == "1");
}

string StringOp::toLower(const string& str)
{
	string result = str;
	transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}

} // namespace openmsx
