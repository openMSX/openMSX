#ifndef CLIOPTION_HH
#define CLIOPTION_HH

#include "array_ref.hh"
#include "string_view.hh"

namespace openmsx {

class CLIOption
{
public:
	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine) = 0;
	virtual void parseDone() {}
	virtual string_view optionHelp() const = 0;

protected:
	~CLIOption() {}
	std::string getArgument(const std::string& option,
	                        array_ref<std::string>& cmdLine) const;
	std::string peekArgument(const array_ref<std::string>& cmdLine) const;
};

class CLIFileType
{
public:
	virtual void parseFileType(const std::string& filename,
	                           array_ref<std::string>& cmdLine) = 0;
	virtual string_view fileTypeHelp() const = 0;

protected:
	~CLIFileType() {}
};

} // namespace openmsx

#endif
