// $Id$

#ifndef INTERPRETEROUTPUT_HH
#define INTERPRETEROUTPUT_HH

#include "string_ref.hh"

namespace openmsx {

class InterpreterOutput
{
public:
	virtual ~InterpreterOutput() {}
	virtual void output(string_ref text) = 0;
	virtual unsigned getOutputColumns() const = 0;
};

} // namespace openmsx

#endif
