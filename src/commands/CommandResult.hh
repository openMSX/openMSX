// $Id$

#ifndef __COMMANDRESULT_HH__
#define __COMMANDRESULT_HH__

#include <string>
#include "openmsx.hh"

using std::string;

namespace openmsx {

class CommandResult
{
public:
	virtual ~CommandResult() {}
	
	virtual void setString(const string& value) = 0;
	virtual void setInt(int value) = 0;
	virtual void setDouble(double value) = 0;
	virtual void setBinary(byte* buf, unsigned length) = 0;
	virtual void addListElement(const string& element) = 0;
};

} // namespace openmsx

#endif
