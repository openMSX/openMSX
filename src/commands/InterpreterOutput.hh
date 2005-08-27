// $Id$

#ifndef INTERPRETEROUTPUT_HH
#define INTERPRETEROUTPUT_HH

#include <string>

namespace openmsx {

class InterpreterOutput
{
public:
	virtual ~InterpreterOutput() {}
	virtual void output(const std::string& text) = 0;
};

} // namespace openmsx

#endif
