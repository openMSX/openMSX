#ifndef NOWINDCOMMAND_HH
#define NOWINDCOMMAND_HH

#include "NowindInterface.hh"
#include "Command.hh"
#include "string_ref.hh"
#include <memory>

namespace openmsx {

class DiskChanger;
class MSXMotherBoard;

class NowindCommand final : public Command
{
public:
	NowindCommand(const std::string& basename,
	              CommandController& commandController,
	              NowindInterface& interface);
	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	std::unique_ptr<DiskChanger> createDiskChanger(
		const std::string& basename, unsigned n,
		MSXMotherBoard& motherBoard) const;

private:
	unsigned searchRomdisk(const NowindInterface::Drives& drives) const;
	void processHdimage(string_ref hdimage,
	                    NowindInterface::Drives& drives) const;
	NowindInterface& interface;
};

} // namespace openmsx

#endif
