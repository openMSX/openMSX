#ifndef CASSETTEPLAYERCOMMAND_HH
#define CASSETTEPLAYERCOMMAND_HH

#include "RecordedCommand.hh"

namespace openmsx {

class CassettePlayer;
class CommandController;
class StateChangeDistributor;
class Scheduler;

class CassettePlayerCommand final : public RecordedCommand
{
public:
	CassettePlayerCommand(CassettePlayer* cassettePlayer_,
		    CommandController& commandController,
		    StateChangeDistributor& stateChangeDistributor,
		    Scheduler& scheduler);
	void execute(std::span<const TclObject> tokens, TclObject& result,
		     EmuTime::param time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
	[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const override;
private:
	CassettePlayer* cassettePlayer;
};

} // namespace openmsx

#endif
