#ifndef CLIOPTION_HH
#define CLIOPTION_HH

#include <span>
#include <string_view>

namespace openmsx {

class CLIOption
{
public:
	CLIOption(const CLIOption&) = delete;
	CLIOption& operator=(const CLIOption&) = delete;

	virtual void parseOption(const std::string& option,
	                         std::span<std::string>& cmdLine) = 0;
	virtual void parseDone() {}
	[[nodiscard]] virtual std::string_view optionHelp() const = 0;

protected:
	CLIOption() = default;
	~CLIOption() = default;
	[[nodiscard]] static std::string getArgument(
		const std::string& option, std::span<std::string>& cmdLine);
	[[nodiscard]] static std::string peekArgument(const std::span<std::string>& cmdLine);
};

class CLIFileType
{
public:
	CLIFileType(const CLIFileType&) = delete;
	CLIFileType& operator=(const CLIFileType&) = delete;

	virtual void parseFileType(const std::string& filename,
	                           std::span<std::string>& cmdLine) = 0;
	[[nodiscard]] virtual std::string_view fileTypeCategoryName() const = 0;
	[[nodiscard]] virtual std::string_view fileTypeHelp() const = 0;

protected:
	CLIFileType() = default;
	~CLIFileType() = default;
};

} // namespace openmsx

#endif
