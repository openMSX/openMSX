#ifndef INTERPRETEROUTPUT_HH
#define INTERPRETEROUTPUT_HH

#include <span>
#include <string_view>

namespace openmsx {

class InterpreterOutput
{
public:
	virtual void output(std::string_view text) = 0;
	[[nodiscard]] virtual unsigned getOutputColumns() const = 0;

	virtual void setCompletions(std::span<const std::string_view> completions) = 0;

protected:
	~InterpreterOutput() = default;
};

} // namespace openmsx

#endif
