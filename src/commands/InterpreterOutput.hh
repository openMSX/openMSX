#ifndef INTERPRETEROUTPUT_HH
#define INTERPRETEROUTPUT_HH

#include "CompletionCandidate.hh"

#include <string_view>
#include <vector>

namespace openmsx {

class InterpreterOutput
{
public:
	virtual void output(std::string_view text) = 0;
	[[nodiscard]] virtual unsigned getOutputColumns() const = 0;

	virtual void setCompletions(std::vector<CompletionCandidate> completions) = 0;

protected:
	~InterpreterOutput() = default;
};

} // namespace openmsx

#endif
