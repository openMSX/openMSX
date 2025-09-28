#ifndef NOWINDCOMMAND_HH
#define NOWINDCOMMAND_HH

#include "NowindHost.hh"

#include "Command.hh"

#include <memory>

namespace openmsx {

class NowindInterface;
class DiskChanger;
class MSXMotherBoard;

class NowindCommand final : public Command
{
public:
	NowindCommand(const std::string& basename,
	              CommandController& commandController,
	              NowindInterface& interface);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	[[nodiscard]] std::unique_ptr<DiskChanger> createDiskChanger(
		const std::string& basename, unsigned n,
		MSXMotherBoard& motherBoard) const;

private:
	void processHdimage(const std::string& hdImage,
	                    NowindHost::Drives& drives) const;

private:
	NowindInterface& interface;
};

} // namespace openmsx

#endif
