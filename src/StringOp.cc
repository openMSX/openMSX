//$Id$

#include <sstream>
#include "StringOp.hh"

using std::ostringstream;

namespace openmsx {

string StringOp::intToString(int num)
{
	ostringstream s;
	s << num;
	return s.str();
}

} // namespace openmsx
