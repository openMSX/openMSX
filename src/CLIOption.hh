#ifndef CLIOPTION_HH
#define CLIOPTION_HH

#include "span.hh"
#include <string_view>

namespace openmsx {

class CLIOption
{
public:
	virtual void parseOption(const std::string& option,
	                         span<std::string>& cmdLine) = 0;
	virtual void parseDone() {}
	virtual std::string_view optionHelp() const = 0;

protected:
	~CLIOption() = default;
	std::string getArgument(const std::string& option,
	                        span<std::string>& cmdLine) const;
	std::string peekArgument(const span<std::string>& cmdLine) const;
};

class CLIFileType
{
public:
	virtual void parseFileType(const std::string& filename,
	                           span<std::string>& cmdLine) = 0;
	virtual std::string_view fileTypeCategoryName() const = 0;
	virtual std::string_view fileTypeHelp() const = 0;

protected:
	~CLIFileType() = default;
};

} // namespace openmsx

#endif
