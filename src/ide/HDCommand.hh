#ifndef HDCOMMAND_HH
#define HDCOMMAND_HH

#include "RecordedCommand.hh"
#include <string>
#include <vector>

namespace openmsx {

class CommandController;
class StateChangeDistributor;
class Scheduler;
class TclObject;
class HD;
class BooleanSetting;

class HDCommand final : public RecordedCommand
{
public:
	HDCommand(CommandController& commandController,
	          StateChangeDistributor& stateChangeDistributor,
	          Scheduler& scheduler, HD& hd, BooleanSetting& powerSetting);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result, EmuTime::param time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const override;
private:
	HD& hd;
	const BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif
