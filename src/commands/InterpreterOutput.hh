#ifndef INTERPRETEROUTPUT_HH
#define INTERPRETEROUTPUT_HH

#include "string_view.hh"

namespace openmsx {

class InterpreterOutput
{
public:
	virtual void output(string_view text) = 0;
	virtual unsigned getOutputColumns() const = 0;

protected:
	~InterpreterOutput() {}
};

} // namespace openmsx

#endif
