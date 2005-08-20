//$Id$

#include <cstdlib>
#include <algorithm>
#include "StringOp.hh"

using std::advance;
using std::equal;
using std::string;
using std::transform;

namespace openmsx {

namespace StringOp {

int stringToInt(const string& str)
{
	return strtol(str.c_str(), NULL, 0);
}

bool stringToBool(const string& str)
{
	string low = StringOp::toLower(str);
	return (low == "true") || (low == "yes") || (low == "1");
}

double stringToDouble(const string& str)
{
	return strtod(str.c_str(), NULL);
}

string toLower(const string& str)
{
	string result = str;
	transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}

bool startsWith(const string& total, const string& part)
{
	if (total.size() < part.size()) return false;
	return equal(part.begin(), part.end(), total.begin());
}

bool endsWith(const string& total, const string& part)
{
	int offset = total.size() - part.size();
	if (offset < 0) return false;
	return equal(part.begin(), part.end(), total.begin() + offset);
}

void trimRight(std::string& str, const std::string& chars)
{
	string::size_type pos = str.find_last_not_of(chars);
	if (pos != string::npos) {
		str.erase(pos + 1);
	} else {
		str.clear();
	}
}

void trimLeft (std::string& str, const std::string& chars)
{
	str.erase(0, str.find_first_not_of(chars));
}

} // namespace StringOp

} // namespace openmsx
