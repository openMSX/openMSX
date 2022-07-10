#ifndef HDIMAGECLI_HH
#define HDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class HDImageCLI final : public CLIOption
{
public:
	explicit HDImageCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 std::span<std::string>& cmdLine) override;
	void parseDone() override;
	[[nodiscard]] std::string_view optionHelp() const override;

	[[nodiscard]] static std::string getImageForId(int id);

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif
